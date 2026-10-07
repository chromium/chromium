// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_MIXED_CONTENT_SETTINGS_TAB_HELPER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_MIXED_CONTENT_SETTINGS_TAB_HELPER_H_

#include <map>
#include <memory>
#include <optional>

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabInterface;
}

// Controls mixed content related settings for the associated WebContents,
// working as the browser version of the mixed content state kept by
// ContentSettingsObserver in the renderer.
class MixedContentSettingsTabHelper : public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(MixedContentSettingsTabHelper);

  static MixedContentSettingsTabHelper* From(tabs::TabInterface* tab);
  static MixedContentSettingsTabHelper* FromWebContents(
      content::WebContents* web_contents);

  MixedContentSettingsTabHelper(tabs::TabInterface& tab,
                                content::WebContents* web_contents);
  explicit MixedContentSettingsTabHelper(content::WebContents* tab);
  MixedContentSettingsTabHelper(const MixedContentSettingsTabHelper&) = delete;
  MixedContentSettingsTabHelper& operator=(
      const MixedContentSettingsTabHelper&) = delete;

  ~MixedContentSettingsTabHelper() override;

  // Enables running active mixed content resources in the associated
  // WebContents/tab. This will stick around as long as the main frame's
  // SiteInstance stays the same. When the SiteInstance changes, we're back to
  // the default (mixed content resources are not allowed to run). See also the
  // `SiteSettings` class below.
  void AllowRunningOfInsecureContent(
      content::RenderFrameHost& render_frame_host);

  bool IsRunningInsecureContentAllowed(
      content::RenderFrameHost& render_frame_host);

 private:
  // content::WebContentsObserver
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  // The SiteSettings is shared between all RenderFrameHosts that uses the
  // `render_frame_host`'s SiteInstance, and the SiteSettings will remain in the
  // `settings_` map as long as it is still used by at least 1 RenderFrame.
  class SiteSettings {
   public:
    explicit SiteSettings(content::RenderFrameHost* render_frame_host);
    SiteSettings(const SiteSettings&) = delete;
    void operator=(const SiteSettings&) = delete;

    void AllowRunningOfInsecureContent();

    bool is_running_insecure_content_allowed() const {
      return is_running_insecure_content_allowed_;
    }

    void IncrementRenderFrameCount();
    void DecrementRenderFrameCount();
    bool render_frame_count() const { return render_frame_count_; }

   private:
    int render_frame_count_ = 0;
    bool is_running_insecure_content_allowed_ = false;
  };

  std::map<raw_ptr<content::SiteInstance>, std::unique_ptr<SiteSettings>>
      settings_;

  std::optional<ui::ScopedUnownedUserData<MixedContentSettingsTabHelper>>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_MIXED_CONTENT_SETTINGS_TAB_HELPER_H_
