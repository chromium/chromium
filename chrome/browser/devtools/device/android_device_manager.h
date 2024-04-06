// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_ANDROID_DEVICE_MANAGER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_ANDROID_DEVICE_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace net {
class StreamSocket;
}

class AndroidDeviceManager {
 public:
  using CommandCallback = base::OnceCallback<void(int, const std::string&)>;
  using SocketCallback =
      base::OnceCallback<void(int result, std::unique_ptr<net::StreamSocket>)>;
  // |body_head| should contain the body (WebSocket frame data) part that has
  // been read during processing the header (WebSocket handshake).
  using HttpUpgradeCallback =
      base::OnceCallback<void(int result,
                              const std::string& extensions,
                              const std::string& body_head,
                              std::unique_ptr<net::StreamSocket>)>;
  using SerialsCallback = base::OnceCallback<void(std::vector<std::string>)>;

  struct BrowserInfo {
    BrowserInfo();
    BrowserInfo(const BrowserInfo& other);
    BrowserInfo& operator=(const BrowserInfo& other);

    enum Type {
      kTypeChrome,
      kTypeWebView,
      kTypeOther
    };

    std::string socket_name;
    std::string display_name;
    std::string user;
    Type type;
  };

  struct DeviceInfo {
    DeviceInfo();
    DeviceInfo(const DeviceInfo& other);
    ~DeviceInfo();

    std::string model;
    bool connected;
    gfx::Size screen_size;
    std::vector<BrowserInfo> browser_info;
  };

  using DeviceInfoCallback = base::OnceCallback<void(const DeviceInfo&)>;
  class Device;

  class AndroidWebSocket {
   public:
    class Delegate {
     public:
      virtual void OnSocketOpened() = 0;
      virtual void OnFrameRead(const std::string& message) = 0;
      virtual void OnSocketClosed() = 0;

     protected:
      virtual ~Delegate() {}
    };

    AndroidWebSocket(const AndroidWebSocket&) = delete;
    AndroidWebSocket& operator=(const AndroidWebSocket&) = delete;

    ~AndroidWebSocket();

    void SendFrame(const std::string& message);

   private:
    friend class Device;
    class WebSocketImpl;

    AndroidWebSocket(
        scoped_refptr<Device> device,
        const std::string& socket_name,
        const std::string& path,
        AndroidWebSocket::Delegate* delegate);
    void Connected(int result,
                   const std::string& extensions,
                   const std::string& body_head,
                   std::unique_ptr<net::StreamSocket> socket);
    void OnFrameRead(const std::string& message);
    void OnSocketClosed();

    scoped_refptr<Device> device_;
    std::unique_ptr<WebSocketImpl, base::OnTaskRunnerDeleter> socket_impl_;
    raw_ptr<Delegate> delegate_;
    base::WeakPtrFactory<AndroidWebSocket> weak_factory_{this};
  };

  class DeviceProvider;

  class Device final : public base::RefCountedDeleteOnSequence<Device> {
   public:
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    void QueryDeviceInfo(DeviceInfoCallback callback);

    void OpenSocket(const std::string& socket_name, SocketCallback callback);

    void SendJsonRequest(const std::string& socket_name,
                         const std::string& request,
                         CommandCallback callback);

    void HttpUpgrade(const std::string& socket_name,
                     const std::string& path,
                     const std::string& extensions,
                     HttpUpgradeCallback callback);
    AndroidWebSocket* CreateWebSocket(
        const std::string& socket_name,
        const std::string& path,
        AndroidWebSocket::Delegate* delegate);

    const std::string& serial() { return serial_; }

   private:
    friend class base::RefCountedDeleteOnSequence<Device>;
    friend class base::DeleteHelper<Device>;
    friend class AndroidDeviceManager;
    friend class AndroidWebSocket;

    Device(scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
           scoped_refptr<DeviceProvider> provider,
           const std::string& serial);
    ~Device();

    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
    scoped_refptr<DeviceProvider> provider_;
    const std::string serial_;

    base::WeakPtrFactory<Device> weak_factory_{this};
  };

  using Devices = std::vector<scoped_refptr<Device>>;
  using DevicesCallback = base::OnceCallback<void(const Devices&)>;

  class DeviceProvider : public base::RefCountedThreadSafe<DeviceProvider> {
   public:
    using SerialsCallback = AndroidDeviceManager::SerialsCallback;
    using DeviceInfoCallback = AndroidDeviceManager::DeviceInfoCallback;
    using SocketCallback = AndroidDeviceManager::SocketCallback;
    using CommandCallback = AndroidDeviceManager::CommandCallback;

    virtual void QueryDevices(SerialsCallback callback) = 0;

    virtual void QueryDeviceInfo(const std::string& serial,
                                 DeviceInfoCallback callback) = 0;

    virtual void OpenSocket(const std::string& serial,
                            const std::string& socket_name,
                            SocketCallback callback) = 0;

    void SendJsonRequest(const std::string& serial,
                         const std::string& socket_name,
                         const std::string& request,
                         CommandCallback callback);

    virtual void HttpUpgrade(const std::string& serial,
                             const std::string& socket_name,
                             const std::string& path,
                             const std::string& extensions,
                             HttpUpgradeCallback callback);

    virtual void ReleaseDevice(const std::string& serial);

   protected:
    friend class base::RefCountedThreadSafe<DeviceProvider>;
    DeviceProvider();
    virtual ~DeviceProvider();
  };

  typedef std::vector<scoped_refptr<DeviceProvider> > DeviceProviders;

  virtual ~AndroidDeviceManager();

  static std::unique_ptr<AndroidDeviceManager> Create();

  void SetDeviceProviders(const DeviceProviders& providers);

  void QueryDevices(DevicesCallback callback);
  void CountDevices(base::OnceCallback<void(int)> callback);

  void set_usb_device_manager_for_test(
      mojo::PendingRemote<device::mojom::UsbDeviceManager> fake_usb_manager);

  static std::string GetBrowserName(const std::string& socket,
                                    const std::string& package);
  using RunCommandCallback =
      base::OnceCallback<void(const std::string&, CommandCallback)>;

  static void QueryDeviceInfo(RunCommandCallback command_callback,
                              DeviceInfoCallback callback);

  struct DeviceDescriptor {
    DeviceDescriptor();
    DeviceDescriptor(const DeviceDescriptor& other);
    ~DeviceDescriptor();

    scoped_refptr<DeviceProvider> provider;
    std::string serial;
  };

  typedef std::vector<DeviceDescriptor> DeviceDescriptors;

 private:
  class HandlerThread {
   public:
    static HandlerThread* GetInstance();
    scoped_refptr<base::SingleThreadTaskRunner> message_loop();

   private:
    friend class base::NoDestructor<HandlerThread>;
    static void StopThread(base::Thread* thread);

    HandlerThread();
    ~HandlerThread();
    raw_ptr<base::Thread> thread_;
  };

  AndroidDeviceManager();

  void UpdateDevices(DevicesCallback callback,
                     std::unique_ptr<DeviceDescriptors> descriptors);

  typedef std::map<std::string, base::WeakPtr<Device> > DeviceWeakMap;

  raw_ptr<HandlerThread> handler_thread_;
  DeviceProviders providers_;
  DeviceWeakMap devices_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AndroidDeviceManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_ANDROID_DEVICE_MANAGER_H_
