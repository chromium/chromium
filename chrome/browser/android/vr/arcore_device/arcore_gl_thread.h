// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_GL_THREAD_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_GL_THREAD_H_

#include <memory>
#include "base/android/java_handler_thread.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/android/vr/mailbox_to_surface_bridge.h"

namespace device {

class ArCoreGl;
class ArImageTransportFactory;

class ArCoreGlThread : public base::android::JavaHandlerThread {
 public:
  ArCoreGlThread(
      std::unique_ptr<ArImageTransportFactory> ar_image_transport_factory,
      std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge,
      base::OnceCallback<void()> initialized_callback);
  ~ArCoreGlThread() override;
  ArCoreGl* GetArCoreGl();

 protected:
  void Init() override;
  void CleanUp() override;

 private:
  std::unique_ptr<ArImageTransportFactory> ar_image_transport_factory_;
  std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge_;
  base::OnceCallback<void()> initialized_callback_;

  // Created on GL thread.
  std::unique_ptr<ArCoreGl> arcore_gl_;

  DISALLOW_COPY_AND_ASSIGN(ArCoreGlThread);
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_GL_THREAD_H_
