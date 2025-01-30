// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_PAGE_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_PAGE_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_

#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/custom_handlers/protocol_handler.h"
#include "content/public/browser/web_contents_observer.h"

using StorageType =
    content_settings::mojom::ContentSettingsManager::StorageType;

class PageSpecificContentSettingsDelegate
    : public content_settings::PageSpecificContentSettings::Delegate,
      public content::WebContentsObserver,
      public MediaStreamCaptureIndicator::Observer {
 public:
  explicit PageSpecificContentSettingsDelegate(
      content::WebContents* web_contents);
  ~PageSpecificContentSettingsDelegate() override;
  PageSpecificContentSettingsDelegate(
      const PageSpecificContentSettingsDelegate&) = delete;
  PageSpecificContentSettingsDelegate& operator=(
      const PageSpecificContentSettingsDelegate&) = delete;

  static PageSpecificContentSettingsDelegate* FromWebContents(
      content::WebContents* web_contents);

  // Call to indicate that there is a protocol handler pending user approval.
  void set_pending_protocol_handler(
      const custom_handlers::ProtocolHandler& handler) {
    pending_protocol_handler_ = handler;
  }

  const custom_handlers::ProtocolHandler& pending_protocol_handler() const {
    return pending_protocol_handler_;
  }

  void ClearPendingProtocolHandler() {
    pending_protocol_handler_ =
        custom_handlers::ProtocolHandler::EmptyProtocolHandler();
  }

  // Sets the previous protocol handler which will be replaced by the
  // pending protocol handler.
  void set_previous_protocol_handler(
      const custom_handlers::ProtocolHandler& handler) {
    previous_protocol_handler_ = handler;
  }

  const custom_handlers::ProtocolHandler& previous_protocol_handler() const {
    return previous_protocol_handler_;
  }

  // Set whether the setting for the pending handler is DEFAULT (ignore),
  // ALLOW, or DENY.
  void set_pending_protocol_handler_setting(ContentSetting setting) {
    pending_protocol_handler_setting_ = setting;
  }

  ContentSetting pending_protocol_handler_setting() const {
    return pending_protocol_handler_setting_;
  }

  // MediaStreamCaptureIndicator::Observer
  void OnIsCapturingVideoChanged(content::WebContents* web_contents,
                                 bool is_capturing_video) override;
  void OnIsCapturingAudioChanged(content::WebContents* web_contents,
                                 bool is_capturing_audio) override;

 private:
  // PageSpecificContentSettings::Delegate:
  void UpdateLocationBar() override;
  PrefService* GetPrefs() override;
  HostContentSettingsMap* GetSettingsMap() override;
  std::unique_ptr<BrowsingDataModel::Delegate> CreateBrowsingDataModelDelegate()
      override;
  void SetDefaultRendererContentSettingRules(
      content::RenderFrameHost* rfh,
      RendererContentSettingRules* rules) override;
  content_settings::PageSpecificContentSettings::MicrophoneCameraState
  GetMicrophoneCameraState() override;
  content::WebContents* MaybeGetSyncedWebContentsForPictureInPicture(
      content::WebContents* web_contents) override;
  void OnContentAllowed(ContentSettingsType type) override;
  void OnContentBlocked(ContentSettingsType type) override;
  bool IsBlockedOnSystemLevel(ContentSettingsType type) override;
  bool IsFrameAllowlistedForJavaScript(
      content::RenderFrameHost* render_frame_host) override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  // Notify `PageSpecificContentSettings` about changes in capturing audio and
  // video.
  void OnCapturingStateChanged(content::WebContents* web_contents,
                               ContentSettingsType type,
                               bool is_capturing);

  // The pending protocol handler, if any. This can be set if
  // registerProtocolHandler was invoked without user gesture.
  // The |IsEmpty| method will be true if no protocol handler is
  // pending registration.
  custom_handlers::ProtocolHandler pending_protocol_handler_ =
      custom_handlers::ProtocolHandler::EmptyProtocolHandler();

  // The previous protocol handler to be replaced by
  // the pending_protocol_handler_, if there is one. Empty if
  // there is no handler which would be replaced.
  custom_handlers::ProtocolHandler previous_protocol_handler_ =
      custom_handlers::ProtocolHandler::EmptyProtocolHandler();

  // It subscribes to Camera and Microphone capturing updates. It is used to
  // show/hide camera/mic activity indicators.
  base::ScopedObservation<MediaStreamCaptureIndicator,
                          MediaStreamCaptureIndicator::Observer>
      media_observation_{this};

  // The setting on the pending protocol handler registration. Persisted in case
  // the user opens the bubble and makes changes multiple times.
  ContentSetting pending_protocol_handler_setting_ = CONTENT_SETTING_DEFAULT;
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_PAGE_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_
