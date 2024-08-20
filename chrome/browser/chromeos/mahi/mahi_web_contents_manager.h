// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_MAHI_WEB_CONTENTS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_MAHI_WEB_CONTENTS_MANAGER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/mahi/mahi_browser_client_impl.h"
#include "chrome/browser/chromeos/mahi/mahi_browser_util.h"
#include "chrome/browser/chromeos/mahi/mahi_content_extraction_delegate.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chromeos/crosapi/mojom/mahi.mojom-forward.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/browser/web_contents_observer.h"
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

// MahiPDFObserver is a helper class that observes the accessibility change from
// the PDF. The accessibility updates will then be used to extract the content
// of PDFs.
class MahiPDFObserver : public content::WebContentsObserver {
 public:
  using PDFContentObservedCallback =
      base::OnceCallback<void(const std::vector<ui::AXTreeUpdate>&)>;
  MahiPDFObserver(content::WebContents* web_contents,
                  ui::AXMode accessibility_mode,
                  ui::AXTreeID tree_id,
                  PDFContentObservedCallback callback);
  MahiPDFObserver(const MahiPDFObserver&) = delete;
  MahiPDFObserver& operator=(const MahiPDFObserver&) = delete;
  ~MahiPDFObserver() override;

  // content::WebContentsObserver:
  void AccessibilityEventReceived(
      const ui::AXUpdatesAndEvents& details) override;

 private:
  // Timer to stop the observation if it's taking too long.
  void OnTimerFired();
  base::OneShotTimer timer_;

  // ID of the tree that contains the PDF.
  const ui::AXTreeID tree_id_;
  // Callback to extract the content from  update.
  PDFContentObservedCallback callback_;
  // Store the updates of the tree that contain the PDF.
  std::vector<ui::AXTreeUpdate> updates_;
  // Enables the accessibility mode for PDF content.
  std::unique_ptr<content::ScopedAccessibilityMode> scoped_accessibility_mode_;

  base::WeakPtrFactory<MahiPDFObserver> weak_ptr_factory_{this};
};

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
  // Passes the `top_level_window` downstream if it is set. This may suppress
  // the notification if it is a media app window that is observed by media app
  // content manager.
  void ClearFocusedWebContentState(
      raw_ptr<aura::Window> top_level_window = nullptr);

  // Clears the focused web content and its state if the focused content is
  // destroyed.
  void WebContentsDestroyed(content::WebContents* web_contents);

  // Called when the browser context menu has been clicked by the user.
  // `question` is used only if `ButtonType` is kQA.
  // Virtual so we can override in tests.
  virtual void OnContextMenuClicked(int64_t display_id,
                                    chromeos::mahi::ButtonType button_type,
                                    const std::u16string& question,
                                    const gfx::Rect& mahi_menu_bounds);

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

  void OnGetInnerText(
      const base::UnguessableToken& page_id,
      const base::Time& start_time,
      std::unique_ptr<content_extraction::InnerTextResult> result);

  void OnGetSnapshot(const base::UnguessableToken& page_id,
                     content::WebContents* web_contents,
                     const base::Time& start_time,
                     GetContentCallback callback,
                     ui::AXTreeUpdate& snapshot);

  void OnFinishDistillableCheck(const base::UnguessableToken& page_id,
                                bool is_distillable);

  // Callback function of the user request from the OS side to get the page
  // content.
  void RequestContent(const base::UnguessableToken& page_id,
                      GetContentCallback callback);

  // Get the page content of normal web pages.
  void RequestWebContent(const base::UnguessableToken& page_id,
                         GetContentCallback callback);

  // Get the content of PDFs.
  void RequestPDFContent(const base::UnguessableToken& page_id,
                         GetContentCallback callback);

  // Process the AXTreeUpdates received for PDF contents.
  void OnGetAXTreeUpdatesForPDF(GetContentCallback callback,
                                const std::vector<ui::AXTreeUpdate>& updates);

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
  raw_ptr<content::WebContents> focused_web_contents_;

  // Store if the current focused web contents is PDF.
  bool is_pdf_focused_web_contents_ = false;

  // Observer to observe accessibility changed on PDFs.
  std::unique_ptr<MahiPDFObserver> pdf_observer_;

  base::WeakPtrFactory<MahiWebContentsManager> weak_pointer_factory_{this};
};

}  // namespace mahi

#endif  // CHROME_BROWSER_CHROMEOS_MAHI_MAHI_WEB_CONTENTS_MANAGER_H_
