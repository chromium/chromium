// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_MEET_BROWSER_MEET_BROWSER_SERVICE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_MEET_BROWSER_MEET_BROWSER_SERVICE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_observer.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_adaptor.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_browser.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::cfm {

// Implementation of the MeetBrowser Service
// The Render Frame Host Token is used to identify the peripheral's device path
// given a hashed device id.
// Note: This control must be initialised in the relevant browser context
// that the Meet Client is running on e.g. Ash or LaCrOS and pass its
// RenderFrameHostToken
class MeetBrowserService : public CfmObserver,
                           public chromeos::cfm::ServiceAdaptor::Delegate,
                           public mojom::MeetBrowser {
 public:
  MeetBrowserService(const MeetBrowserService&) = delete;
  MeetBrowserService& operator=(const MeetBrowserService&) = delete;

  // Manage singleton instance.
  static void Initialize();
  static void Shutdown();
  static MeetBrowserService* Get();
  static bool IsInitialized();

  // Sets the content::GlobalRenderFrameToken for meet used to determine the
  // content::RenderFrameHost
  void SetMeetGlobalRenderFrameToken(
      const content::GlobalRenderFrameHostToken& host_token);

 protected:
  // mojom::CfmObserver implementation
  bool ServiceRequestReceived(const std::string& interface_name) override;

  // chromeos::cfm::ServiceAdaptor::Delegate implementation
  void OnBindService(mojo::ScopedMessagePipeHandle receiver_pipe) override;
  void OnAdaptorConnect(bool success) override;
  void OnAdaptorDisconnect() override;

  // mojom:MeetBrowser implementation
  void TranslateVideoDeviceId(const std::string& hashed_device_id,
                              TranslateVideoDeviceIdCallback callback) override;

 private:
  MeetBrowserService();
  ~MeetBrowserService() override;

  chromeos::cfm::ServiceAdaptor service_adaptor_;
  mojo::ReceiverSet<mojom::MeetBrowser> receivers_;
  content::GlobalRenderFrameHostToken host_token_;
  base::WeakPtrFactory<MeetBrowserService> weak_factory_{this};
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_MEET_BROWSER_MEET_BROWSER_SERVICE_H_
