// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_MAHI_WEB_CONTENTS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_MAHI_WEB_CONTENTS_MANAGER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/mahi/mahi_browser_client_impl.h"
#include "chrome/browser/chromeos/mahi/mahi_browser_util.h"
#include "chrome/browser/chromeos/mahi/mahi_content_extraction_delegate.h"
#include "chromeos/crosapi/mojom/mahi.mojom-forward.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace mahi {

class ScopedMahiWebContentsManagerForTesting;
class MockMahiWebContentsManager;
class FakeMahiWebContentsManager;

using GetContentCallback =
    base::OnceCallback<void(crosapi::mojom::MahiPageContentPtr)>;

// `MahiWebContentsManager` is the central class for mahi web contents in the
// browser (ash and lacros) responsible for:
// 1. Being the single source of truth for mahi browser parameters like focused
//    web page, the lasted extracted web page, etc. and providing this
//    information to ChromeOS backend as needed.
// 2. Decides the distillability of a web page and extract contents from a
//    requested web page.
class MahiWebContentsManager {
 public:
  MahiWebContentsManager(const MahiWebContentsManager&) = delete;
  MahiWebContentsManager& operator=(const MahiWebContentsManager&) = delete;

  static MahiWebContentsManager* Get();

  void Initialize();

  // Called when the focused tab finish loading.
  // Virtual so we can override in tests.
  virtual void OnFocusedPageLoadComplete(content::WebContents* web_contents);

  // Clears the focused web content state, and notifies mahi manager.
  void ClearFocusedWebContentState();

  // Called when the browser context menu has been clicked by the user.
  // `question` is used only if `ButtonType` is kQA.
  // Virtual so we can override in tests.
  virtual void OnContextMenuClicked(int64_t display_id,
                                    ButtonType button_type,
                                    const std::u16string& question);

  // Returns boolean to indicate if the current focused page is distillable. The
  // default return is false, for the cases when the focused page's
  // distillability has not been checked yet.
  bool IsFocusedPageDistillable();

  // Virtual so that can be overridden in test.
  virtual bool GetPrefValue() const;
  void set_mahi_pref_lacros(bool value) { mahi_pref_lacros_ = value; }

 private:
  friend base::NoDestructor<MahiWebContentsManager>;
  // Friends to access some test-only functions.
  friend class ScopedMahiWebContentsManagerForTesting;
  friend class MockMahiWebContentsManager;
  friend class FakeMahiWebContentsManager;

  static void SetInstanceForTesting(MahiWebContentsManager* manager);
  static void ResetInstanceForTesting();

  MahiWebContentsManager();
  virtual ~MahiWebContentsManager();

  void OnGetSnapshot(const base::UnguessableToken& page_id,
                     content::WebContents* web_contents,
                     const base::Time& start_time,
                     const ui::AXTreeUpdate& snapshot);

  void OnFinishDistillableCheck(const base::UnguessableToken& page_id,
                                bool is_distillable);

  // Callback function of the user request from the OS side to get the page
  // content.
  void RequestContent(const base::UnguessableToken& page_id,
                      GetContentCallback callback);

  // Should be called when user requests on the focused page. We should update
  // the focused page state to the requested page state.
  void FocusedPageGotRequest();

  // Gets the favicon from the given web contents. Returns an empty imageskia if
  // there is no valid one.
  // Virtual so we can override in tests.
  virtual gfx::ImageSkia GetFavicon(content::WebContents* web_contents) const;

  // Determines if the given web contents should be skipped for distillability
  // check.
  bool ShouldSkip(content::WebContents* web_contents);

  std::unique_ptr<MahiContentExtractionDelegate> content_extraction_delegate_;
  std::unique_ptr<MahiBrowserClientImpl> client_;
  bool is_initialized_ = false;

  bool mahi_pref_lacros_ = false;

  // The state of the web content which get focus in the browser.
  WebContentState focused_web_content_state_{/*url=*/GURL(), /*title=*/u""};
  // The state of the web content which is requested by the user.
  WebContentState requested_web_content_state_{/*url=*/GURL(), /*title=*/u""};

  base::WeakPtrFactory<MahiWebContentsManager> weak_pointer_factory_{this};
};

}  // namespace mahi

#endif  // CHROME_BROWSER_CHROMEOS_MAHI_MAHI_WEB_CONTENTS_MANAGER_H_
