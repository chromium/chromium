// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_LACROS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_LACROS_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chromeos/crosapi/mojom/dlp.mojom.h"
#include "content/public/browser/desktop_media_id.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {
class WebContents;
}  // namespace content

namespace policy {

// LaCros-wide class that tracks the set of currently known confidential
// WebContents and whether any of them are currently visible.
class DlpContentManagerLacros : public DlpContentManager,
                                public aura::WindowObserver {
 public:
  // Creates the instance if not yet created.
  // There will always be a single instance created on the first access.
  static DlpContentManagerLacros* Get();

  // DlpContentManager overrides:
  void CheckScreenShareRestriction(const content::DesktopMediaID& media_id,
                                   const std::u16string& application_title,
                                   WarningCallback callback) override;
  void OnScreenShareStarted(
      const std::string& label,
      std::vector<content::DesktopMediaID> screen_share_ids,
      const std::u16string& application_title,
      base::RepeatingClosure stop_callback,
      content::MediaStreamUI::StateChangeCallback state_change_callback,
      content::MediaStreamUI::SourceCallback source_callback) override;
  void OnScreenShareStopped(const std::string& label,
                            const content::DesktopMediaID& media_id) override;
  void TabLocationMaybeChanged(content::WebContents* web_contents) override;

 private:
  friend class DlpContentManagerTestHelper;
  friend class DlpContentObserver;

  // Class that tracks connection with screen share tracking in Ash.
  class ScreenShareStateChangeDelegate
      : public crosapi::mojom::StateChangeDelegate {
   public:
    ScreenShareStateChangeDelegate(
        const std::string& label,
        const content::DesktopMediaID& media_id,
        content::MediaStreamUI::StateChangeCallback state_change_callback,
        base::OnceClosure stop_callback);
    ScreenShareStateChangeDelegate(const ScreenShareStateChangeDelegate&) =
        delete;
    ScreenShareStateChangeDelegate& operator=(
        const ScreenShareStateChangeDelegate&) = delete;
    ~ScreenShareStateChangeDelegate() override;

    bool operator==(const ScreenShareStateChangeDelegate& other) const;
    bool operator!=(const ScreenShareStateChangeDelegate& other) const;

    mojo::PendingRemote<crosapi::mojom::StateChangeDelegate> BindDelegate();

    // crosapi::mojom::StateChangeDelegate overrides:
    void OnPause() override;
    void OnResume() override;
    void OnStop() override;

    const std::string& label() const { return label_; }
    const content::DesktopMediaID& media_id() const { return media_id_; }

   private:
    const std::string label_;
    const content::DesktopMediaID media_id_;
    content::MediaStreamUI::StateChangeCallback state_change_callback_;
    base::OnceClosure stop_callback_;
    mojo::Receiver<crosapi::mojom::StateChangeDelegate> receiver_{this};
  };

  DlpContentManagerLacros();
  ~DlpContentManagerLacros() override;

  // DlpContentObserver overrides:
  void OnConfidentialityChanged(
      content::WebContents* web_contents,
      const DlpContentRestrictionSet& restriction_set) override;
  void OnWebContentsDestroyed(content::WebContents* web_contents) override;
  void OnVisibilityChanged(content::WebContents* web_contents) override;

  // aura::WindowObserver overrides:
  void OnWindowDestroying(aura::Window* window) override;

  // Updates |confidential_windows_| entry for |window| and notifies Ash if
  // needed.
  void UpdateRestrictions(aura::Window* window);

  // DlpContentManager override:
  ConfidentialContentsInfo GetScreenShareConfidentialContentsInfo(
      const content::DesktopMediaID& media_id,
      content::WebContents* web_contents) const override;

  // Tracks set of known confidential WebContents* for each Window*.
  base::flat_map<aura::Window*, base::flat_set<content::WebContents*>>
      window_webcontents_;

  // Tracks current restrictions applied to Window* based on visible
  // WebContents* belonging to Window*.
  base::flat_map<aura::Window*, DlpContentRestrictionSet> confidential_windows_;

  // List of currently running screen shares that are tracked remotely in Ash.
  std::vector<std::unique_ptr<ScreenShareStateChangeDelegate>>
      running_remote_screen_shares_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_LACROS_H_
