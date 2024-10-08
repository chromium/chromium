// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_MAHI_WEB_CONTENTS_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_MAHI_WEB_CONTENTS_MANAGER_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/mahi/mahi_content_extraction_delegate.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chromeos/components/mahi/public/cpp/mahi_browser_util.h"
#include "chromeos/components/mahi/public/cpp/mahi_util.h"
#include "chromeos/components/mahi/public/cpp/mahi_web_contents_manager.h"
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

class MockMahiWebContentsManager;
class FakeMahiWebContentsManager;

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

class MahiWebContentsManagerImpl : public chromeos::MahiWebContentsManager {
 public:
  MahiWebContentsManagerImpl(const MahiWebContentsManagerImpl&) = delete;
  MahiWebContentsManagerImpl& operator=(const MahiWebContentsManagerImpl&) =
      delete;
  MahiWebContentsManagerImpl();
  ~MahiWebContentsManagerImpl() override;

  // chromeos::MahiWebContentsManager:
  void OnFocusedPageLoadComplete(content::WebContents* web_contents) override;
  void ClearFocusedWebContentState() override;
  void WebContentsDestroyed(content::WebContents* web_contents) override;
  void OnContextMenuClicked(int64_t display_id,
                            chromeos::mahi::ButtonType button_type,
                            const std::u16string& question,
                            const gfx::Rect& mahi_menu_bounds) override;
  bool IsFocusedPageDistillable() override;
  void RequestContent(const base::UnguessableToken& page_id,
                      chromeos::mahi::GetContentCallback callback) override;

 private:
  // Friends to access some test-only functions.
  friend class MockMahiWebContentsManager;
  friend class FakeMahiWebContentsManager;

  void OnGetInnerText(
      const base::UnguessableToken& page_id,
      const base::Time& start_time,
      std::unique_ptr<content_extraction::InnerTextResult> result);

  void OnGetSnapshot(const base::UnguessableToken& page_id,
                     content::WebContents* web_contents,
                     const base::Time& start_time,
                     chromeos::mahi::GetContentCallback callback,
                     ui::AXTreeUpdate& snapshot);

  void OnFinishDistillableCheck(const base::UnguessableToken& page_id,
                                bool is_distillable);

  // Get the page content of normal web pages.
  void RequestWebContent(const base::UnguessableToken& page_id,
                         chromeos::mahi::GetContentCallback callback);

  // Get the content of PDFs.
  void RequestPDFContent(const base::UnguessableToken& page_id,
                         chromeos::mahi::GetContentCallback callback);

  // Process the AXTreeUpdates received for PDF contents.
  void OnGetAXTreeUpdatesForPDF(chromeos::mahi::GetContentCallback callback,
                                const std::vector<ui::AXTreeUpdate>& updates);

  // Gets the favicon from the given web contents. Returns an empty imageskia if
  // there is no valid one.
  // Virtual so we can override in tests.
  virtual gfx::ImageSkia GetFavicon(content::WebContents* web_contents) const;

  // Determines if the given web contents should be skipped for distillability
  // check.
  bool ShouldSkip(content::WebContents* web_contents);

  std::unique_ptr<MahiContentExtractionDelegate> content_extraction_delegate_;

  // The state of the web content which get focus in the browser.
  WebContentState focused_web_content_state_{/*url=*/GURL(), /*title=*/u""};
  raw_ptr<content::WebContents> focused_web_contents_;

  // Store if the current focused web contents is PDF.
  bool is_pdf_focused_web_contents_ = false;

  // Observer to observe accessibility changed on PDFs.
  std::unique_ptr<MahiPDFObserver> pdf_observer_;

  base::WeakPtrFactory<MahiWebContentsManagerImpl> weak_pointer_factory_{this};
};

}  // namespace mahi

#endif  // CHROME_BROWSER_CHROMEOS_MAHI_MAHI_WEB_CONTENTS_MANAGER_IMPL_H_
