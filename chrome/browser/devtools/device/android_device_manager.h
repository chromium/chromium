// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_ANDROID_DEVICE_MANAGER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_ANDROID_DEVICE_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace net {
class StreamSocket;
}

class AndroidDeviceManager {
 public:
  using CommandCallback =
      base::Callback<void(int, const std::string&)>;
  using SocketCallback =
      base::Callback<void(int result, std::unique_ptr<net::StreamSocket>)>;
  // |body_head| should contain the body (WebSocket frame data) part that has
  // been read during processing the header (WebSocket handshake).
  using HttpUpgradeCallback =
      base::Callback<void(int result,
                          const std::string& extensions,
                          const std::string& body_head,
                          std::unique_ptr<net::StreamSocket>)>;
  using SerialsCallback =
      base::Callback<void(const std::vector<std::string>&)>;

  struct BrowserInfo {
    BrowserInfo();
    BrowserInfo(const BrowserInfo& other);

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

  typedef base::Callback<void(const DeviceInfo&)> DeviceInfoCallback;
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
    Delegate* delegate_;
    base::WeakPtrFactory<AndroidWebSocket> weak_factory_{this};
    DISALLOW_COPY_AND_ASSIGN(AndroidWebSocket);
  };

  class DeviceProvider;

  class Device final : public base::RefCountedDeleteOnSequence<Device> {
   public:
    void QueryDeviceInfo(const DeviceInfoCallback& callback);

    void OpenSocket(const std::string& socket_name,
                    const SocketCallback& callback);

    void SendJsonRequest(const std::string& socket_name,
                         const std::string& request,
                         const CommandCallback& callback);

    void HttpUpgrade(const std::string& socket_name,
                     const std::string& path,
                     const std::string& extensions,
                     const HttpUpgradeCallback& callback);
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

    DISALLOW_COPY_AND_ASSIGN(Device);
  };

  typedef std::vector<scoped_refptr<Device> > Devices;
  typedef base::Callback<void(const Devices&)> DevicesCallback;

  class DeviceProvider : public base::RefCountedThreadSafe<DeviceProvider> {
   public:
    typedef AndroidDeviceManager::SerialsCallback SerialsCallback;
    typedef AndroidDeviceManager::DeviceInfoCallback DeviceInfoCallback;
    typedef AndroidDeviceManager::SocketCallback SocketCallback;
    typedef AndroidDeviceManager::CommandCallback CommandCallback;

    virtual void QueryDevices(const SerialsCallback& callback) = 0;

    virtual void QueryDeviceInfo(const std::string& serial,
                                 const DeviceInfoCallback& callback) = 0;

    virtual void OpenSocket(const std::string& serial,
                            const std::string& socket_name,
                            const SocketCallback& callback) = 0;

    virtual void SendJsonRequest(const std::string& serial,
                                 const std::string& socket_name,
                                 const std::string& request,
                                 const CommandCallback& callback);

    virtual void HttpUpgrade(const std::string& serial,
                             const std::string& socket_name,
                             const std::string& path,
                             const std::string& extensions,
                             const HttpUpgradeCallback& callback);

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

  void QueryDevices(const DevicesCallback& callback);
  void CountDevices(const base::Callback<void(int)>& callback);

  void set_usb_device_manager_for_test(
      mojo::PendingRemote<device::mojom::UsbDeviceManager> fake_usb_manager);

  static std::string GetBrowserName(const std::string& socket,
                                    const std::string& package);
  using RunCommandCallback =
      base::Callback<void(const std::string&, const CommandCallback&)>;

  static void QueryDeviceInfo(const RunCommandCallback& command_callback,
                              const DeviceInfoCallback& callback);

  struct DeviceDescriptor {
    DeviceDescriptor();
    DeviceDescriptor(const DeviceDescriptor& other);
    ~DeviceDescriptor();

    scoped_refptr<DeviceProvider> provider;
    std::string serial;
  };

  typedef std::vector<DeviceDescriptor> DeviceDescriptors;

 private:
  class HandlerThread : public base::RefCountedThreadSafe<HandlerThread> {
   public:
    static scoped_refptr<HandlerThread> GetInstance();
    scoped_refptr<base::SingleThreadTaskRunner> message_loop();

   private:
    friend class base::RefCountedThreadSafe<HandlerThread>;
    static HandlerThread* instance_;
    static void StopThread(base::Thread* thread);

    HandlerThread();
    virtual ~HandlerThread();
    base::Thread* thread_;
  };

  AndroidDeviceManager();

  void UpdateDevices(const DevicesCallback& callback,
                     std::unique_ptr<DeviceDescriptors> descriptors);

  typedef std::map<std::string, base::WeakPtr<Device> > DeviceWeakMap;

  scoped_refptr<HandlerThread> handler_thread_;
  DeviceProviders providers_;
  DeviceWeakMap devices_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AndroidDeviceManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_ANDROID_DEVICE_MANAGER_H_
