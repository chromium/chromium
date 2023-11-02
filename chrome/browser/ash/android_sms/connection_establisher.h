// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ANDROID_SMS_CONNECTION_ESTABLISHER_H_
#define CHROME_BROWSER_ASH_ANDROID_SMS_CONNECTION_ESTABLISHER_H_

#include "content/public/browser/service_worker_context.h"
#include "url/gurl.h"

namespace ash {
namespace android_sms {

// Establishes a background connection from the Android Messages for Web
// service worker to the Tachyon server.
class ConnectionEstablisher {
 public:
  enum class ConnectionMode { kStartConnection, kResumeExistingConnection };

  ConnectionEstablisher(const ConnectionEstablisher&) = delete;
  ConnectionEstablisher& operator=(const ConnectionEstablisher&) = delete;

  virtual ~ConnectionEstablisher() = default;

  virtual void EstablishConnection(
      const GURL& url,
      ConnectionMode connection_mode,
      content::ServiceWorkerContext* service_worker_context) = 0;

  virtual void TearDownConnection(
      const GURL& url,
      content::ServiceWorkerContext* service_worker_context) = 0;

 protected:
  ConnectionEstablisher() = default;
};

}  // namespace android_sms
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ANDROID_SMS_CONNECTION_ESTABLISHER_H_
