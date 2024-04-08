// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/android_device_manager.h"

#include <stddef.h>
#include <string.h>

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chrome/browser/devtools/device/usb/usb_device_manager_helper.h"
#include "chrome/browser/devtools/device/usb/usb_device_provider.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
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
    AndroidDeviceManager::DeviceInfoCallback callback,
    const AndroidDeviceManager::DeviceInfo& device_info) {
  response_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), device_info));
}

static void PostCommandCallback(
    scoped_refptr<base::SingleThreadTaskRunner> response_task_runner,
    AndroidDeviceManager::CommandCallback callback,
    int result,
    const std::string& response) {
  response_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result, response));
}

static void PostHttpUpgradeCallback(
    scoped_refptr<base::SingleThreadTaskRunner> response_task_runner,
    AndroidDeviceManager::HttpUpgradeCallback callback,
    int result,
    const std::string& extensions,
    const std::string& body_head,
    std::unique_ptr<net::StreamSocket> socket) {
  response_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result, extensions,
                                body_head, std::move(socket)));
}

class HttpRequest {
 public:
  typedef AndroidDeviceManager::CommandCallback CommandCallback;
  typedef AndroidDeviceManager::HttpUpgradeCallback HttpUpgradeCallback;

  static void CommandRequest(const std::string& path,
                             CommandCallback callback,
                             int result,
                             std::unique_ptr<net::StreamSocket> socket) {
    if (result != net::OK) {
      std::move(callback).Run(result, std::string());
      return;
    }
    new HttpRequest(std::move(socket), path, {}, std::move(callback));
  }

  static void HttpUpgradeRequest(const std::string& path,
                                 const std::string& extensions,
                                 HttpUpgradeCallback callback,
                                 int result,
                                 std::unique_ptr<net::StreamSocket> socket) {
    if (result != net::OK) {
      std::move(callback).Run(result, std::string(), std::string(),
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
    new HttpRequest(std::move(socket), path, headers, std::move(callback));
  }

 private:
  HttpRequest(std::unique_ptr<net::StreamSocket> socket,
              const std::string& path,
              const std::map<std::string, std::string>& headers,
              CommandCallback callback)
      : socket_(std::move(socket)),
        command_callback_(std::move(callback)),
        expected_total_size_(0) {
    SendRequest(path, headers);
  }

  HttpRequest(std::unique_ptr<net::StreamSocket> socket,
              const std::string& path,
              const std::map<std::string, std::string>& headers,
              HttpUpgradeCallback callback)
      : socket_(std::move(socket)),
        http_upgrade_callback_(std::move(callback)),
        expected_total_size_(0) {
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

      result = socket_->Write(request_.get(), request_->BytesRemaining(),
                              base::BindOnce(&HttpRequest::DoSendRequest,
                                             weak_ptr_factory_.GetWeakPtr()),
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

    std::vector<std::string_view> pieces = {requestLine, crlf};
    for (const auto& header_and_value : headers) {
      pieces.insert(pieces.end(), {header_and_value.first, colon,
                                   header_and_value.second, crlf});
    }
    pieces.insert(pieces.end(), {crlf});

    std::string request = base::StrCat(pieces);
    auto base_buffer =
        base::MakeRefCounted<net::IOBufferWithSize>(request.size());
    memcpy(base_buffer->data(), request.data(), request.size());
    request_ = base::MakeRefCounted<net::DrainableIOBuffer>(
        std::move(base_buffer), request.size());
    timeout_timer_.Start(
        FROM_HERE, base::Seconds(1),
        base::BindOnce(&HttpRequest::Die, weak_ptr_factory_.GetWeakPtr(),
                       net::ERR_TIMED_OUT));
    DoSendRequest(net::OK);
  }

  void ReadResponse(int result) {
    if (!CheckNetResultOrDie(result))
      return;

    response_buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);

    result = socket_->Read(response_buffer_.get(), kBufferSize,
                           base::BindOnce(&HttpRequest::OnResponseData,
                                          weak_ptr_factory_.GetWeakPtr()));
    if (result != net::ERR_IO_PENDING)
      OnResponseData(result);
  }

  void OnResponseData(int result) {
    scoped_refptr<net::HttpResponseHeaders> headers = nullptr;
    size_t header_size;

    do {
      if (!CheckNetResultOrDie(result))
        return;
      if (result == 0) {
        CheckNetResultOrDie(net::ERR_CONNECTION_CLOSED);
        return;
      }

      response_.append(response_buffer_->data(), result);

      if (!headers) {
        header_size = response_.find("\r\n\r\n");

        if (header_size != std::string::npos) {
          header_size += 4;
          headers = base::MakeRefCounted<net::HttpResponseHeaders>(
              net::HttpUtil::AssembleRawHeaders(
                  std::string_view(response_.data(), header_size)));

          int expected_body_size = 0;

          std::string content_length;
          if (headers->GetNormalizedHeader("Content-Length", &content_length)) {
            if (!base::StringToInt(content_length, &expected_body_size)) {
              CheckNetResultOrDie(net::ERR_FAILED);
              return;
            }
          }

          expected_total_size_ = header_size + expected_body_size;
        }
      }

      // WebSocket handshake doesn't contain the Content-Length header. For this
      // case, |expected_total_size_| is set to the size of the header (opening
      // handshake).
      //
      // Some (part of) WebSocket frames can be already received into
      // |response_|.
      if (headers && response_.length() >= expected_total_size_) {
        const std::string& body = response_.substr(header_size);

        if (!command_callback_.is_null()) {
          std::move(command_callback_).Run(net::OK, body);
        } else {
          std::string sec_websocket_extensions;
          headers->GetNormalizedHeader("Sec-WebSocket-Extensions",
                                       &sec_websocket_extensions);
          std::move(http_upgrade_callback_)
              .Run(net::OK, sec_websocket_extensions, body,
                   std::move(socket_));
        }

        delete this;
        return;
      }

      result = socket_->Read(response_buffer_.get(), kBufferSize,
                             base::BindOnce(&HttpRequest::OnResponseData,
                                            weak_ptr_factory_.GetWeakPtr()));
    } while (result != net::ERR_IO_PENDING);
  }

  void Die(int result) {
    if (!command_callback_.is_null()) {
      std::move(command_callback_).Run(result, std::string());
    } else {
      std::move(http_upgrade_callback_)
          .Run(result, std::string(), std::string(),
               base::WrapUnique<net::StreamSocket>(nullptr));
    }
    delete this;
  }

  bool CheckNetResultOrDie(int result) {
    if (result >= 0) {
      // Reset the timer whenever a non-error is received from network.
      // It means network is responsive for now, so we reset the timer
      // and wait for a new result until the next timeout.
      timeout_timer_.Reset();
      return true;
    }

    Die(result);
    return false;
  }

  std::unique_ptr<net::StreamSocket> socket_;
  scoped_refptr<net::DrainableIOBuffer> request_;
  std::string response_;
  base::OneShotTimer timeout_timer_;
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
  base::WeakPtrFactory<HttpRequest> weak_ptr_factory_{this};
};

class DevicesRequest : public base::RefCountedThreadSafe<DevicesRequest> {
 public:
  using DeviceInfo = AndroidDeviceManager::DeviceInfo;
  using DeviceProvider = AndroidDeviceManager::DeviceProvider;
  using DeviceProviders = AndroidDeviceManager::DeviceProviders;
  using DeviceDescriptors = AndroidDeviceManager::DeviceDescriptors;
  using DescriptorsCallback =
      base::OnceCallback<void(std::unique_ptr<DeviceDescriptors>)>;

  static void Start(
      scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
      const DeviceProviders& providers,
      DescriptorsCallback callback) {
    // Don't keep counted reference on calling thread;
    scoped_refptr<DevicesRequest> request =
        base::WrapRefCounted(new DevicesRequest(std::move(callback)));
    for (const auto& provider : providers) {
      device_task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&DeviceProvider::QueryDevices, provider,
                         base::BindOnce(&DevicesRequest::ProcessSerials,
                                        request, provider)));
    }
    device_task_runner->ReleaseSoon(FROM_HERE, std::move(request));
  }

 private:
  explicit DevicesRequest(DescriptorsCallback callback)
      : response_task_runner_(
            base::SingleThreadTaskRunner::GetCurrentDefault()),
        callback_(std::move(callback)),
        descriptors_(new DeviceDescriptors()) {}

  friend class base::RefCountedThreadSafe<DevicesRequest>;
  ~DevicesRequest() {
    response_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), std::move(descriptors_)));
  }

  using Serials = std::vector<std::string>;

  void ProcessSerials(scoped_refptr<DeviceProvider> provider, Serials serials) {
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

void OnCountDevices(base::OnceCallback<void(int)> callback, int device_count) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), device_count));
}

}  // namespace

AndroidDeviceManager::BrowserInfo::BrowserInfo()
    : type(kTypeOther) {
}

AndroidDeviceManager::BrowserInfo::BrowserInfo(const BrowserInfo& other) =
    default;

AndroidDeviceManager::BrowserInfo& AndroidDeviceManager::BrowserInfo::operator=(
    const BrowserInfo& other) = default;

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
    CommandCallback callback) {
  OpenSocket(serial, socket_name,
             base::BindOnce(&HttpRequest::CommandRequest, request,
                            std::move(callback)));
}

void AndroidDeviceManager::DeviceProvider::HttpUpgrade(
    const std::string& serial,
    const std::string& socket_name,
    const std::string& path,
    const std::string& extensions,
    HttpUpgradeCallback callback) {
  OpenSocket(serial, socket_name,
             base::BindOnce(&HttpRequest::HttpUpgradeRequest, path, extensions,
                            std::move(callback)));
}

void AndroidDeviceManager::DeviceProvider::ReleaseDevice(
    const std::string& serial) {
}

AndroidDeviceManager::DeviceProvider::DeviceProvider() {
}

AndroidDeviceManager::DeviceProvider::~DeviceProvider() {
}

void AndroidDeviceManager::Device::QueryDeviceInfo(
    DeviceInfoCallback callback) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DeviceProvider::QueryDeviceInfo, provider_, serial_,
          base::BindOnce(&PostDeviceInfoCallback,
                         base::SingleThreadTaskRunner::GetCurrentDefault(),
                         std::move(callback))));
}

void AndroidDeviceManager::Device::OpenSocket(const std::string& socket_name,
                                              SocketCallback callback) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeviceProvider::OpenSocket, provider_, serial_,
                                socket_name, std::move(callback)));
}

void AndroidDeviceManager::Device::SendJsonRequest(
    const std::string& socket_name,
    const std::string& request,
    CommandCallback callback) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DeviceProvider::SendJsonRequest, provider_, serial_, socket_name,
          request,
          base::BindOnce(&PostCommandCallback,
                         base::SingleThreadTaskRunner::GetCurrentDefault(),
                         std::move(callback))));
}

void AndroidDeviceManager::Device::HttpUpgrade(const std::string& socket_name,
                                               const std::string& path,
                                               const std::string& extensions,
                                               HttpUpgradeCallback callback) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DeviceProvider::HttpUpgrade, provider_, serial_, socket_name, path,
          extensions,
          base::BindOnce(&PostHttpUpgradeCallback,
                         base::SingleThreadTaskRunner::GetCurrentDefault(),
                         std::move(callback))));
}

AndroidDeviceManager::Device::Device(
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
    scoped_refptr<DeviceProvider> provider,
    const std::string& serial)
    : RefCountedDeleteOnSequence<Device>(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      task_runner_(device_task_runner),
      provider_(provider),
      serial_(serial) {}

AndroidDeviceManager::Device::~Device() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeviceProvider::ReleaseDevice,
                                std::move(provider_), std::move(serial_)));
}

// static
AndroidDeviceManager::HandlerThread*
AndroidDeviceManager::HandlerThread::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static base::NoDestructor<AndroidDeviceManager::HandlerThread> s_instance;
  return s_instance.get();
}

AndroidDeviceManager::HandlerThread::HandlerThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  thread_ = new base::Thread(kDevToolsAdbBridgeThreadName);
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  if (!thread_->StartWithOptions(std::move(options))) {
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
  if (!thread_)
    return;
  // Shut down thread on a thread other than UI so it can join a thread.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::WithBaseSyncPrimitives(), base::TaskPriority::BEST_EFFORT},
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

void AndroidDeviceManager::QueryDevices(DevicesCallback callback) {
  DevicesRequest::Start(
      handler_thread_->message_loop(), providers_,
      base::BindOnce(&AndroidDeviceManager::UpdateDevices,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AndroidDeviceManager::CountDevices(
    base::OnceCallback<void(int)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  handler_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::BindOnce(&UsbDeviceManagerHelper::CountDevices,
                     base::BindOnce(&OnCountDevices, std::move(callback))));
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
    DevicesCallback callback,
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
  std::move(callback).Run(response);
}
