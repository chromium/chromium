// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_file_picker.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/types/display_constants.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "ui/base/base_window.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#include "ui/aura/window.h"
#endif

using content::DownloadManager;
using content::WebContents;
using download::DownloadItem;

namespace {

bool HasSupportedAuthorityScheme(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile() ||
         url.SchemeIs(url::kFtpScheme);
}

// Extracts an authority-bearing URL from `url`, unwrapping `blob:` and
// `filesystem:` URLs to their inner origin and rejecting authority-less
// schemes such as `about:` and `data:`.
// For `blob:` / `filesystem:`, accepts any valid non-opaque inner origin
// (including `chrome://` and `chrome-extension://`) to mirror
// `download::BaseFile::GetEffectiveAuthorityURL()`.
GURL ExtractAuthorityURL(const GURL& url) {
  if (!url.is_valid()) {
    return GURL();
  }
  if (url.SchemeIsBlob() || url.SchemeIsFileSystem()) {
    return url::Origin::Create(url).GetTupleOrPrecursorTupleIfOpaque().GetURL();
  }
  return HasSupportedAuthorityScheme(url) ? url : GURL();
}

// Extracts a supported authority URL from `origin`, falling back to its
// precursor tuple when `origin` is opaque (e.g. `data:` or sandboxed `srcdoc`
// frames). Uses `SchemeHostPort::GetURL()` so remote `file://host/` origins
// preserve their host instead of collapsing to `file:///`.
GURL ExtractAuthorityURL(const url::Origin& origin) {
  const GURL origin_url = origin.GetTupleOrPrecursorTupleIfOpaque().GetURL();
  return HasSupportedAuthorityScheme(origin_url) ? origin_url : GURL();
}

}  // namespace

DownloadFilePicker::DownloadFilePicker(download::DownloadItem* item,
                                       const base::FilePath& suggested_path,
                                       ConfirmationCallback callback)
    : suggested_path_(suggested_path),
      file_selected_callback_(std::move(callback)),
      download_item_(item) {
  const DownloadPrefs* prefs = DownloadPrefs::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item));
  CHECK(prefs, base::NotFatalUntil::M161);

  CHECK(item, base::NotFatalUntil::M161);
  item->AddObserver(this);
  // Exiting fullscreen may spin a nested loop and destroy |download_item_|.
  if (WebContents* initial_contents =
          content::DownloadItemUtils::GetWebContents(item)) {
    if (auto blocker = initial_contents->ForSecurityDropFullscreen(
            display::kInvalidDisplayId)) {
      fullscreen_block_ = std::move(*blocker);
    }
  }
  WebContents* web_contents =
      download_item_
          ? content::DownloadItemUtils::GetWebContents(download_item_)
          : nullptr;
  // Extension download may not have associated webcontents.
  if (!download_item_ || (web_contents && !fullscreen_block_) ||
      (download_item_->GetDownloadSource() !=
           download::DownloadSource::EXTENSION_API &&
       (!web_contents || !web_contents->GetNativeView()))) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DownloadFilePicker::FileSelectionCanceled,
                                  base::Unretained(this)));
    return;
  }

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));
  // |select_file_dialog_| could be null in Linux. See CreateSelectFileDialog()
  // in shell_dialog_linux.cc.
  if (!select_file_dialog_.get()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DownloadFilePicker::FileSelectionCanceled,
                                  base::Unretained(this)));
    return;
  }

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  // Platform file pickers, notably on Mac and Windows, tend to break
  // with double extensions like .tar.gz, so only pass in normal ones.
  base::FilePath::StringType extension = suggested_path_.FinalExtension();
  if (!extension.empty()) {
    extension.erase(extension.begin());  // drop the .
    file_type_info.extensions.resize(1);
    file_type_info.extensions[0].push_back(extension);
  }
  file_type_info.include_all_files = true;
  file_type_info.allowed_paths =
      ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH;
  gfx::NativeWindow owning_window =
      web_contents ? platform_util::GetTopLevel(web_contents->GetNativeView())
                   : gfx::NativeWindow();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  // On Aura (Linux/Win), an offscreen extension WebContents has a non-null
  // aura::Window* with a null WindowTreeHost, which prevents the file dialog
  // from being modal (https://crbug.com/40825014). Clear it so we fall back to
  // the last active browser window below.
  if (owning_window && !owning_window->GetHost()) {
    owning_window = gfx::NativeWindow();
  }
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  // If the download was initiated without a WebContents (e.g., from an
  // extension background service worker) or with a hostless Aura window, fall
  // back to the last active browser window.
  if (!owning_window) {
    if (BrowserWindowInterface* browser =
            GetLastActiveBrowserWindowInterfaceWithAnyProfile();
        browser && browser->GetWindow()) {
      owning_window = browser->GetWindow()->GetNativeWindow();
    }
  }
#endif

  GURL caller = GetEffectiveAuthorityURL(download_item_);

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(),
      suggested_path_, &file_type_info, 0, base::FilePath::StringType(),
      owning_window, &caller);
}

DownloadFilePicker::~DownloadFilePicker() {
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }

  if (download_item_) {
    download_item_->RemoveObserver(this);
  }
}

void DownloadFilePicker::FileSelected(const ui::SelectedFileInfo& file,
                                      int index) {
  std::move(file_selected_callback_)
      .Run(DownloadConfirmationResult::CONFIRMED, file);

  delete this;
}

void DownloadFilePicker::FileSelectionCanceled() {
  std::move(file_selected_callback_)
      .Run(DownloadConfirmationResult::CANCELED, ui::SelectedFileInfo());

  delete this;
}

// static
void DownloadFilePicker::ShowFilePicker(DownloadItem* item,
                                        const base::FilePath& suggested_path,
                                        ConfirmationCallback callback) {
  new DownloadFilePicker(item, suggested_path, std::move(callback));
  // DownloadFilePicker deletes itself.
}

// static
GURL DownloadFilePicker::GetEffectiveAuthorityURL(const DownloadItem* item) {
  if (!item) {
    return GURL();
  }

  // 1. Prefer an authority-bearing download URL directly (`http:`, `https:`,
  //    `file:`, `ftp:`, or unwrapped `blob:` / `filesystem:` inner origin)
  //    before consulting initiator or referrer metadata.
  if (GURL url = ExtractAuthorityURL(item->GetURL()); url.is_valid()) {
    return url;
  }

  // 2. Mirror `BaseFile::GetEffectiveAuthorityURL()`: when no
  //    `request_initiator` was provided, prefer an HTTP/S referrer before
  //    falling back to frame/tab metadata. For browser-initiated navigation
  //    downloads (e.g. omnibox or bookmark `data:` URLs) with neither an
  //    initiator nor an HTTP/S referrer, do not attribute the download to the
  //    tab's previously committed document.
  if (!item->GetRequestInitiator().has_value()) {
    if (item->GetReferrerUrl().is_valid() &&
        item->GetReferrerUrl().SchemeIsHTTPOrHTTPS()) {
      return item->GetReferrerUrl();
    }
    if (item->GetDownloadSource() == download::DownloadSource::NAVIGATION) {
      return GURL();
    }
  }

  WebContents* web_contents = content::DownloadItemUtils::GetWebContents(item);
  content::RenderFrameHost* render_frame_host =
      content::DownloadItemUtils::GetRenderFrameHost(item);

  // 3. Extract authority-bearing URLs from the initiating frame and tab,
  //    preferring the live committed tab entry's URL over `item->GetTabUrl()`
  //    (which may snapshot an uncommitted pending navigation entry).
  const GURL raw_frame_url =
      render_frame_host ? render_frame_host->GetLastCommittedURL() : GURL();
  const GURL frame_url = ExtractAuthorityURL(raw_frame_url);
  const content::NavigationEntry* committed_entry =
      web_contents ? web_contents->GetController().GetLastCommittedEntry()
                   : nullptr;
  const GURL raw_tab_url =
      (committed_entry && committed_entry->GetURL().is_valid())
          ? committed_entry->GetURL()
          : item->GetTabUrl();
  const GURL tab_url = ExtractAuthorityURL(raw_tab_url);

  // 4. Prefer the browser-validated initiator's (or initiating frame's)
  //    committed/precursor origin when available. If the initiating frame or
  //    tab carries a path-bearing URL with that same origin, return it so
  //    path-based DLP rules still match.
  GURL initiator_url;
  if (item->GetRequestInitiator().has_value()) {
    initiator_url = ExtractAuthorityURL(*item->GetRequestInitiator());
  }
  if (!initiator_url.is_valid() && render_frame_host) {
    initiator_url =
        ExtractAuthorityURL(render_frame_host->GetLastCommittedOrigin());
  }
  if (initiator_url.is_valid()) {
    const url::Origin initiator_origin = url::Origin::Create(initiator_url);
    const bool frame_is_origin_only =
        raw_frame_url.SchemeIsBlob() || raw_frame_url.SchemeIsFileSystem();
    if (frame_url.is_valid() && !frame_is_origin_only &&
        initiator_origin.IsSameOriginWith(frame_url)) {
      return frame_url;
    }
    if (tab_url.is_valid() && initiator_origin.IsSameOriginWith(tab_url)) {
      return tab_url;
    }
    return initiator_url;
  }

  // 5. Fall back to the normalized initiating frame or tab authority URL (e.g.
  //    `blob:chrome-extension://` frames or tab-only download items).
  if (frame_url.is_valid()) {
    return frame_url;
  }
  if (tab_url.is_valid()) {
    return tab_url;
  }

  // 6. For authority-less tabs such as `window.open('about:blank')`, fall back
  //    to the primary main frame's inherited or precursor authority origin.
  if (web_contents && web_contents->GetPrimaryMainFrame()) {
    return ExtractAuthorityURL(
        web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  }

  return GURL();
}

void DownloadFilePicker::OnDownloadDestroyed(DownloadItem* download_item) {
  CHECK_EQ(download_item, download_item_, base::NotFatalUntil::M161);
  download_item_ = nullptr;
}
