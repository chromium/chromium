// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/android_device_manager.h"

#include <stddef.h>
#include <string.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/devtools/device/usb/usb_device_manager_helper.h"
#include "chrome/browser/devtools/device/usb/usb_device_provider.h"
#include "content/public/browser/browser_task_traits.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

using content::BrowserThread;

namespace {

const char kDevToolsAdbBridgeThreadName[] = "Chrome_DevToolsADBThread";

const int kBufferSize = 16 * 1024;

static const char kModelOffline[] = "Offline";

static const char kRequestLineFormat[] = "GET %s HTTP/1.1";

net::NetworkTrafficAnnotationTag kAndroidDeviceManagerTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("android_device_manager_socket", R"(
        semantics {
          sender: "Android Device Manager"
          description:
            "Remote debugging is supported over existing ADB (Android Debug "
            "Bridge) connection, in addition to raw USB connection. This "
            "socket talks to the local ADB daemon which routes debugging "
            "traffic to a remote device."
          trigger:
            "A user connects to an Android device using remote debugging."
          data: "Any data required for remote debugging."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting:
            "To use ADB with a device connected over USB, you must enable USB "
            "debugging in the device system settings, under Developer options."
          policy_exception_justification:
            "This is not a network request and is only used for remote "
            "debugging."
        })");

static void PostDeviceInfoCallback(
    scoped_refptr<base::SingleThreadTaskRunner> response_task_runner,
    const AndroidDeviceManager::DeviceInfoCallback& callback,
    const AndroidDeviceManager::DeviceInfo& device_info) {
  response_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(callback, device_info));
}

static void PostCommandCallback(
    scoped_refptr<base::SingleThreadTaskRunner> response_task_runner,
    const AndroidDeviceManager::CommandCallback& callback,
    int result,
    const std::string& response) {
  response_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(callback, result, response));
}

static void PostHttpUpgradeCallback(
    scoped_refptr<base::SingleThreadTaskRunner> response_task_runner,
    const AndroidDeviceManager::HttpUpgradeCallback& callback,
    int result,
    const std::string& extensions,
    const std::string& body_head,
    std::unique_ptr<net::StreamSocket> socket) {
  response_task_runner->PostTask(
      FROM_HERE, base::BindOnce(callback, result, extensions, body_head,
                                std::move(socket)));
}

class HttpRequest {
 public:
  typedef AndroidDeviceManager::CommandCallback CommandCallback;
  typedef AndroidDeviceManager::HttpUpgradeCallback HttpUpgradeCallback;

  static void CommandRequest(const std::string& path,
                             const CommandCallback& callback,
                             int result,
                             std::unique_ptr<net::StreamSocket> socket) {
    if (result != net::OK) {
      callback.Run(result, std::string());
      return;
    }
    new HttpRequest(std::move(socket), path, {}, callback);
  }

  static void HttpUpgradeRequest(const std::string& path,
                                 const std::string& extensions,
                                 const HttpUpgradeCallback& callback,
                                 int result,
                                 std::unique_ptr<net::StreamSocket> socket) {
    if (result != net::OK) {
      callback.Run(result, std::string(), std::string(),
                   base::WrapUnique<net::StreamSocket>(nullptr));
      return;
    }
    std::map<std::string, std::string> headers = {
        {"Upgrade", "WebSocket"},
        {"Connection", "Upgrade"},
        {"Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ=="},
        {"Sec-WebSocket-Version", "13"}};
    if (!extensions.empty())
      headers["Sec-WebSocket-Extensions"] = extensions;
    new HttpRequest(std::move(socket), path, headers, callback);
  }

 private:
  HttpRequest(std::unique_ptr<net::StreamSocket> socket,
              const std::string& path,
              const std::map<std::string, std::string>& headers,
              const CommandCallback& callback)
      : socket_(std::move(socket)),
        command_callback_(callback),
        expected_total_size_(0),
        header_size_(std::string::npos) {
    SendRequest(path, headers);
  }

  HttpRequest(std::unique_ptr<net::StreamSocket> socket,
              const std::string& path,
              const std::map<std::string, std::string>& headers,
              const HttpUpgradeCallback& callback)
      : socket_(std::move(socket)),
        http_upgrade_callback_(callback),
        expected_total_size_(0),
        header_size_(std::string::npos) {
    SendRequest(path, headers);
  }

  ~HttpRequest() {
  }

  void DoSendRequest(int result) {
    while (result != net::ERR_IO_PENDING) {
      if (!CheckNetResultOrDie(result))
        return;

      if (result > 0)
        request_->DidConsume(result);

      if (request_->BytesRemaining() == 0) {
        request_ = nullptr;
        ReadResponse(net::OK);
        return;
      }

      result = socket_->Write(
          request_.get(), request_->BytesRemaining(),
          base::Bind(&HttpRequest::DoSendRequest, base::Unretained(this)),
          kAndroidDeviceManagerTrafficAnnotation);
    }
  }

  void SendRequest(const std::string& path,
                   std::map<std::string, std::string> headers) {
    net::IPEndPoint remote_address;
    socket_->GetPeerAddress(&remote_address);
    headers["Host"] = remote_address.ToString();

    std::string requestLine =
        base::StringPrintf(kRequestLineFormat, path.c_str());
    std::string crlf = "\r\n";
    std::string colon = ": ";

    std::vector<base::StringPiece> pieces = {requestLine, crlf};
    for (const auto& header_and_value : headers) {
      pieces.insert(pieces.end(), {header_and_value.first, colon,
                                   header_and_value.second, crlf});
    }
    pieces.insert(pieces.end(), {crlf});

    std::string request = base::StrCat(pieces);
    scoped_refptr<net::IOBuffer> base_buffer =
        base::MakeRefCounted<net::IOBuffer>(request.size());
    memcpy(base_buffer->data(), request.data(), request.size());
    request_ = base::MakeRefCounted<net::DrainableIOBuffer>(
        std::move(base_buffer), request.size());

    DoSendRequest(net::OK);
  }

  void ReadResponse(int result) {
    if (!CheckNetResultOrDie(result))
      return;

    response_buffer_ = base::MakeRefCounted<net::IOBuffer>(kBufferSize);

    result = socket_->Read(
        response_buffer_.get(),
        kBufferSize,
        base::Bind(&HttpRequest::OnResponseData, base::Unretained(this)));
    if (result != net::ERR_IO_PENDING)
      OnResponseData(result);
  }

  void OnResponseData(int result) {
    do {
      if (!CheckNetResultOrDie(result))
        return;
      if (result == 0) {
        CheckNetResultOrDie(net::ERR_CONNECTION_CLOSED);
        return;
      }

      response_.append(response_buffer_->data(), result);

      if (header_size_ == std::string::npos) {
        header_size_ = response_.find("\r\n\r\n");

        if (header_size_ != std::string::npos) {
          header_size_ += 4;

          int expected_body_size = 0;

          // TODO(kaznacheev): Use net::HttpResponseHeader to parse the header.
          std::string content_length = ExtractHeader("Content-Length:");
          if (!content_length.empty()) {
            if (!base::StringToInt(content_length, &expected_body_size)) {
              CheckNetResultOrDie(net::ERR_FAILED);
              return;
            }
          }

          expected_total_size_ = header_size_ + expected_body_size;
        }
      }

      // WebSocket handshake doesn't contain the Content-Length header. For this
      // case, |expected_total_size_| is set to the size of the header (opening
      // handshake).
      //
      // Some (part of) WebSocket frames can be already received into
      // |response_|.
      if (header_size_ != std::string::npos &&
          response_.length() >= expected_total_size_) {
        const std::string& body = response_.substr(header_size_);

        if (!command_callback_.is_null()) {
          command_callback_.Run(net::OK, body);
        } else {
          http_upgrade_callback_.Run(net::OK,
                                     ExtractHeader("Sec-WebSocket-Extensions:"),
                                     body, std::move(socket_));
        }

        delete this;
        return;
      }

      result = socket_->Read(
          response_buffer_.get(), kBufferSize,
          base::Bind(&HttpRequest::OnResponseData, base::Unretained(this)));
    } while (result != net::ERR_IO_PENDING);
  }

  std::string ExtractHeader(const std::string& header) {
    size_t start_pos = response_.find(header);
    if (start_pos == std::string::npos)
      return std::string();

    size_t endline_pos = response_.find("\n", start_pos);
    if (endline_pos == std::string::npos)
      return std::string();

    std::string value = response_.substr(
        start_pos + header.length(), endline_pos - start_pos - header.length());
    base::TrimWhitespaceASCII(value, base::TRIM_ALL, &value);
    return value;
  }

  bool CheckNetResultOrDie(int result) {
    if (result >= 0)
      return true;
    if (!command_callback_.is_null()) {
      command_callback_.Run(result, std::string());
    } else {
      http_upgrade_callback_.Run(result, std::string(), std::string(),
                                 base::WrapUnique<net::StreamSocket>(nullptr));
    }
    delete this;
    return false;
  }

  std::unique_ptr<net::StreamSocket> socket_;
  scoped_refptr<net::DrainableIOBuffer> request_;
  std::string response_;
  CommandCallback command_callback_;
  HttpUpgradeCallback http_upgrade_callback_;

  scoped_refptr<net::IOBuffer> response_buffer_;

  // Initially set to 0. Once the end of the header is seen:
  // - If the Content-Length header is included, set to the sum of the header
  //   size (including the last two CRLFs) and the value of
  //   the header.
  // - Otherwise, this variable is set to the size of the header (including the
  //   last two CRLFs).
  size_t expected_total_size_;
  // Initially set to std::string::npos. Once the end of the header is seen,
  // set to the size of the header part in |response_| including the two CRLFs
  // at the end.
  size_t header_size_;
};

class DevicesRequest : public base::RefCountedThreadSafe<DevicesRequest> {
 public:
  typedef AndroidDeviceManager::DeviceInfo DeviceInfo;
  typedef AndroidDeviceManager::DeviceProvider DeviceProvider;
  typedef AndroidDeviceManager::DeviceProviders DeviceProviders;
  typedef AndroidDeviceManager::DeviceDescriptors DeviceDescriptors;
  typedef base::Callback<void(std::unique_ptr<DeviceDescriptors>)>
      DescriptorsCallback;

  static void Start(
      scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
      const DeviceProviders& providers,
      const DescriptorsCallback& callback) {
    // Don't keep counted reference on calling thread;
    scoped_refptr<DevicesRequest> request =
        base::WrapRefCounted(new DevicesRequest(callback));
    for (auto it = providers.begin(); it != providers.end(); ++it) {
      device_task_runner->PostTask(
          FROM_HERE, base::BindOnce(&DeviceProvider::QueryDevices, *it,
                                    base::Bind(&DevicesRequest::ProcessSerials,
                                               request, *it)));
    }
    device_task_runner->ReleaseSoon(FROM_HERE, std::move(request));
  }

 private:
  explicit DevicesRequest(const DescriptorsCallback& callback)
      : response_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        callback_(callback),
        descriptors_(new DeviceDescriptors()) {}

  friend class base::RefCountedThreadSafe<DevicesRequest>;
  ~DevicesRequest() {
    response_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(callback_, std::move(descriptors_)));
  }

  typedef std::vector<std::string> Serials;

  void ProcessSerials(scoped_refptr<DeviceProvider> provider,
                      const Serials& serials) {
    for (auto it = serials.begin(); it != serials.end(); ++it) {
      descriptors_->resize(descriptors_->size() + 1);
      descriptors_->back().provider = provider;
      descriptors_->back().serial = *it;
    }
  }

  scoped_refptr<base::SingleThreadTaskRunner> response_task_runner_;
  DescriptorsCallback callback_;
  std::unique_ptr<DeviceDescriptors> descriptors_;
};

void OnCountDevices(const base::Callback<void(int)>& callback,
                    int device_count) {
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(callback, device_count));
}

}  // namespace

AndroidDeviceManager::BrowserInfo::BrowserInfo()
    : type(kTypeOther) {
}

AndroidDeviceManager::BrowserInfo::BrowserInfo(const BrowserInfo& other) =
    default;

AndroidDeviceManager::DeviceInfo::DeviceInfo()
    : model(kModelOffline), connected(false) {
}

AndroidDeviceManager::DeviceInfo::DeviceInfo(const DeviceInfo& other) = default;

AndroidDeviceManager::DeviceInfo::~DeviceInfo() {
}

AndroidDeviceManager::DeviceDescriptor::DeviceDescriptor() {
}

AndroidDeviceManager::DeviceDescriptor::DeviceDescriptor(
    const DeviceDescriptor& other) = default;

AndroidDeviceManager::DeviceDescriptor::~DeviceDescriptor() {
}

void AndroidDeviceManager::DeviceProvider::SendJsonRequest(
    const std::string& serial,
    const std::string& socket_name,
    const std::string& request,
    const CommandCallback& callback) {
  OpenSocket(serial, socket_name,
             base::Bind(&HttpRequest::CommandRequest, request, callback));
}

void AndroidDeviceManager::DeviceProvider::HttpUpgrade(
    const std::string& serial,
    const std::string& socket_name,
    const std::string& path,
    const std::string& extensions,
    const HttpUpgradeCallback& callback) {
  OpenSocket(
      serial, socket_name,
      base::Bind(&HttpRequest::HttpUpgradeRequest, path, extensions, callback));
}

void AndroidDeviceManager::DeviceProvider::ReleaseDevice(
    const std::string& serial) {
}

AndroidDeviceManager::DeviceProvider::DeviceProvider() {
}

AndroidDeviceManager::DeviceProvider::~DeviceProvider() {
}

void AndroidDeviceManager::Device::QueryDeviceInfo(
    const DeviceInfoCallback& callback) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DeviceProvider::QueryDeviceInfo, provider_, serial_,
          base::Bind(&PostDeviceInfoCallback,
                     base::ThreadTaskRunnerHandle::Get(), callback)));
}

void AndroidDeviceManager::Device::OpenSocket(const std::string& socket_name,
                                              const SocketCallback& callback) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeviceProvider::OpenSocket, provider_, serial_,
                                socket_name, callback));
}

void AndroidDeviceManager::Device::SendJsonRequest(
    const std::string& socket_name,
    const std::string& request,
    const CommandCallback& callback) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeviceProvider::SendJsonRequest, provider_,
                                serial_, socket_name, request,
                                base::Bind(&PostCommandCallback,
                                           base::ThreadTaskRunnerHandle::Get(),
                                           callback)));
}

void AndroidDeviceManager::Device::HttpUpgrade(
    const std::string& socket_name,
    const std::string& path,
    const std::string& extensions,
    const HttpUpgradeCallback& callback) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeviceProvider::HttpUpgrade, provider_,
                                serial_, socket_name, path, extensions,
                                base::Bind(&PostHttpUpgradeCallback,
                                           base::ThreadTaskRunnerHandle::Get(),
                                           callback)));
}

AndroidDeviceManager::Device::Device(
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
    scoped_refptr<DeviceProvider> provider,
    const std::string& serial)
    : RefCountedDeleteOnSequence<Device>(base::ThreadTaskRunnerHandle::Get()),
      task_runner_(device_task_runner),
      provider_(provider),
      serial_(serial) {}

AndroidDeviceManager::Device::~Device() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeviceProvider::ReleaseDevice,
                                std::move(provider_), std::move(serial_)));
}

AndroidDeviceManager::HandlerThread*
AndroidDeviceManager::HandlerThread::instance_ = nullptr;

// static
scoped_refptr<AndroidDeviceManager::HandlerThread>
AndroidDeviceManager::HandlerThread::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!instance_)
    new HandlerThread();
  return instance_;
}

AndroidDeviceManager::HandlerThread::HandlerThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  instance_ = this;
  thread_ = new base::Thread(kDevToolsAdbBridgeThreadName);
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  if (!thread_->StartWithOptions(options)) {
    delete thread_;
    thread_ = nullptr;
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
AndroidDeviceManager::HandlerThread::message_loop() {
  return thread_ ? thread_->task_runner() : nullptr;
}

// static
void AndroidDeviceManager::HandlerThread::StopThread(
    base::Thread* thread) {
  delete thread;
}

AndroidDeviceManager::HandlerThread::~HandlerThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  instance_ = nullptr;
  if (!thread_)
    return;
  // Shut down thread on a thread other than UI so it can join a thread.
  base::PostTask(FROM_HERE,
                 {base::ThreadPool(), base::WithBaseSyncPrimitives(),
                  base::TaskPriority::BEST_EFFORT},
                 base::BindOnce(&HandlerThread::StopThread, thread_));
}

// static
std::unique_ptr<AndroidDeviceManager> AndroidDeviceManager::Create() {
  return base::WrapUnique(new AndroidDeviceManager());
}

void AndroidDeviceManager::SetDeviceProviders(
    const DeviceProviders& providers) {
  for (auto it = providers_.begin(); it != providers_.end(); ++it) {
    handler_thread_->message_loop()->ReleaseSoon(FROM_HERE, std::move(*it));
  }
  providers_ = providers;
}

void AndroidDeviceManager::QueryDevices(const DevicesCallback& callback) {
  DevicesRequest::Start(handler_thread_->message_loop(), providers_,
                        base::Bind(&AndroidDeviceManager::UpdateDevices,
                                   weak_factory_.GetWeakPtr(), callback));
}

void AndroidDeviceManager::CountDevices(
    const base::Callback<void(int)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  handler_thread_->message_loop()->PostTask(
      FROM_HERE, base::BindOnce(&UsbDeviceManagerHelper::CountDevices,
                                base::BindOnce(&OnCountDevices, callback)));
}

void AndroidDeviceManager::set_usb_device_manager_for_test(
    mojo::PendingRemote<device::mojom::UsbDeviceManager> fake_usb_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  handler_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::BindOnce(&UsbDeviceManagerHelper::SetUsbManagerForTesting,
                     std::move(fake_usb_manager)));
}

AndroidDeviceManager::AndroidDeviceManager()
    : handler_thread_(HandlerThread::GetInstance()) {}

AndroidDeviceManager::~AndroidDeviceManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetDeviceProviders(DeviceProviders());
}

void AndroidDeviceManager::UpdateDevices(
    const DevicesCallback& callback,
    std::unique_ptr<DeviceDescriptors> descriptors) {
  Devices response;
  DeviceWeakMap new_devices;
  for (DeviceDescriptors::const_iterator it = descriptors->begin();
       it != descriptors->end();
       ++it) {
    auto found = devices_.find(it->serial);
    scoped_refptr<Device> device;
    if (found == devices_.end() || !found->second ||
        found->second->provider_.get() != it->provider.get()) {
      device =
          new Device(handler_thread_->message_loop(), it->provider, it->serial);
    } else {
      device = found->second.get();
    }
    response.push_back(device);
    new_devices[it->serial] = device->weak_factory_.GetWeakPtr();
  }
  devices_.swap(new_devices);
  callback.Run(response);
}
