// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_dialog_controller.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/memory/weak_ptr.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/print_view_manager_base.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/win/conflicts/module_database.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/arc/print_spooler/print_session_impl.h"
#endif

using content::NavigationController;
using content::NavigationHandle;
using content::WebContents;
using content::WebUIMessageHandler;

namespace printing {

namespace {

PrintPreviewUI* GetPrintPreviewUIForDialog(WebContents* dialog) {
  content::WebUI* web_ui = dialog->GetWebUI();
  return web_ui ? web_ui->GetController()->GetAs<PrintPreviewUI>() : nullptr;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void CloseArcPrintSession(WebContents* initiator) {
  WebContents* outermost_web_contents =
      guest_view::GuestViewBase::GetTopLevelWebContents(initiator);
  auto* arc_print_session =
      arc::PrintSessionImpl::FromWebContents(outermost_web_contents);
  if (arc_print_session)
    arc_print_session->OnPrintPreviewClosed();
}
#endif

// A ui::WebDialogDelegate that specifies the print preview dialog appearance.
class PrintPreviewDialogDelegate : public ui::WebDialogDelegate {
 public:
  explicit PrintPreviewDialogDelegate(WebContents* initiator);

  PrintPreviewDialogDelegate(const PrintPreviewDialogDelegate&) = delete;
  PrintPreviewDialogDelegate& operator=(const PrintPreviewDialogDelegate&) =
      delete;

  ~PrintPreviewDialogDelegate() override;

  ui::mojom::ModalType GetDialogModalType() const override;
  std::u16string GetDialogTitle() const override;
  std::u16string GetAccessibleDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogClosingFromKeyEvent() override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(WebContents* source, bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;

 private:
  WebContents* initiator() const { return web_contents_.get(); }

  base::WeakPtr<content::WebContents> web_contents_;
  bool on_dialog_closed_called_ = false;
};

PrintPreviewDialogDelegate::PrintPreviewDialogDelegate(WebContents* initiator)
    : web_contents_(initiator->GetWeakPtr()) {}

PrintPreviewDialogDelegate::~PrintPreviewDialogDelegate() = default;

ui::mojom::ModalType PrintPreviewDialogDelegate::GetDialogModalType() const {
  // Not used, returning dummy value.
  NOTREACHED_IN_MIGRATION();
  return ui::mojom::ModalType::kWindow;
}

std::u16string PrintPreviewDialogDelegate::GetDialogTitle() const {
  // Only used on Windows? UI folks prefer no title.
  return std::u16string();
}

std::u16string PrintPreviewDialogDelegate::GetAccessibleDialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_PRINT_PREVIEW_TITLE);
}

GURL PrintPreviewDialogDelegate::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIPrintURL);
}

void PrintPreviewDialogDelegate::GetDialogSize(gfx::Size* size) const {
  DCHECK(size);
  const gfx::Size kMinDialogSize(800, 480);
  const int kBorder = 25;
  *size = kMinDialogSize;

  web_modal::WebContentsModalDialogHost* host = nullptr;
  content::WebContents* outermost_web_contents =
      guest_view::GuestViewBase::GetTopLevelWebContents(initiator());
  if (!outermost_web_contents)
    return;

  Browser* browser = chrome::FindBrowserWithTab(outermost_web_contents);
  if (browser)
    host = browser->window()->GetWebContentsModalDialogHost();

  if (host)
    size->SetToMax(host->GetMaximumDialogSize());
  else
    size->SetToMax(outermost_web_contents->GetContainerBounds().size());
  size->Enlarge(-2 * kBorder, -kBorder);

  static const gfx::Size kMaxDialogSize(1000, 660);
  int max_width = std::max(size->width() * 7 / 10, kMaxDialogSize.width());
  int max_height =
      std::max(max_width * kMaxDialogSize.height() / kMaxDialogSize.width(),
               kMaxDialogSize.height());
  size->SetToMin(gfx::Size(max_width, max_height));
}

std::string PrintPreviewDialogDelegate::GetDialogArgs() const {
  return std::string();
}

void PrintPreviewDialogDelegate::OnDialogClosingFromKeyEvent() {
  OnDialogClosed(std::string());
}

void PrintPreviewDialogDelegate::OnDialogClosed(
    const std::string& /* json_retval */) {
  if (on_dialog_closed_called_ || !initiator())
    return;

  on_dialog_closed_called_ = true;

  auto* print_view_manager = PrintViewManager::FromWebContents(initiator());
  if (print_view_manager)
    print_view_manager->PrintPreviewAlmostDone();
}

void PrintPreviewDialogDelegate::OnCloseContents(WebContents* /* source */,
                                                 bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool PrintPreviewDialogDelegate::ShouldShowDialogTitle() const {
  return false;
}

}  // namespace

// PrintPreviewDialogController ------------------------------------------------

PrintPreviewDialogController::PrintPreviewDialogController()
    : web_contents_collection_(this) {}

// static
PrintPreviewDialogController* PrintPreviewDialogController::GetInstance() {
  if (!g_browser_process)
    return nullptr;
  return g_browser_process->print_preview_dialog_controller();
}

void PrintPreviewDialogController::PrintPreview(
    WebContents* initiator,
    const mojom::RequestPrintPreviewParams& params) {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  PrintViewManagerBase::DisableThirdPartyBlocking();
#endif

  if (initiator->IsCrashed()) {
    return;
  }

  if (!GetOrCreatePreviewDialog(initiator, params)) {
    auto* print_view_manager = PrintViewManager::FromWebContents(initiator);
    if (print_view_manager) {
      print_view_manager->PrintPreviewDone();
    }
  }
}

// static
std::unique_ptr<ui::WebDialogDelegate>
PrintPreviewDialogController::CreatePrintPreviewDialogDelegateForTesting(
    WebContents* initiator) {
  return std::make_unique<PrintPreviewDialogDelegate>(initiator);
}

WebContents* PrintPreviewDialogController::GetOrCreatePreviewDialogForTesting(
    WebContents* initiator) {
  mojom::RequestPrintPreviewParams params;
  params.is_modifiable = true;
  return GetOrCreatePreviewDialog(initiator, params);
}

WebContents* PrintPreviewDialogController::GetOrCreatePreviewDialog(
    WebContents* initiator,
    const mojom::RequestPrintPreviewParams& params) {
  DCHECK(initiator);

  // Get the print preview dialog for `initiator`.
  WebContents* preview_dialog = GetPrintPreviewForContents(initiator);
  if (preview_dialog) {
    return preview_dialog;
  }

  // TODO(https://crbug.com/372062070: It's currently probably possible to
  // invoke print-preview on non-tab web contents. There are currently tests
  // that allow this for extension guest-views and chrome apps. For now allow
  // these through. In the future we may want to restrict printing to tabs.
  tabs::TabInterface* tab = tabs::TabInterface::MaybeGetFromContents(initiator);
  if (tab && !tab->CanShowModalUI()) {
    return nullptr;
  }

  return CreatePrintPreviewDialog(tab, initiator, params);
}

WebContents* PrintPreviewDialogController::GetPrintPreviewForContents(
    WebContents* contents) const {
  // `preview_dialog_map_` is keyed by the preview dialog, so if
  // base::Contains() succeeds, then `contents` is the preview dialog.
  if (base::Contains(preview_dialog_map_, contents))
    return contents;

  for (const auto& it : preview_dialog_map_) {
    // If `contents` is an initiator.
    if (contents == it.second.initiator) {
      // Return the associated preview dialog.
      return it.first;
    }
  }
  return nullptr;
}

WebContents* PrintPreviewDialogController::GetInitiator(
    WebContents* preview_dialog) {
  auto it = preview_dialog_map_.find(preview_dialog);
  return it != preview_dialog_map_.end() ? it->second.initiator : nullptr;
}

const mojom::RequestPrintPreviewParams*
PrintPreviewDialogController::GetRequestParams(
    content::WebContents* preview_dialog) const {
  auto it = preview_dialog_map_.find(preview_dialog);
  return it != preview_dialog_map_.end() ? &it->second.request_params : nullptr;
}

void PrintPreviewDialogController::ForEachPreviewDialog(
    base::RepeatingCallback<void(content::WebContents*)> callback) {
  for (const auto& it : preview_dialog_map_)
    callback.Run(it.first);
}

// static
bool PrintPreviewDialogController::IsPrintPreviewURL(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host_piece() == chrome::kChromeUIPrintHost;
}

// static
bool PrintPreviewDialogController::IsPrintPreviewContentURL(const GURL& url) {
  return url.SchemeIs(content::kChromeUIUntrustedScheme) &&
         url.host_piece() == chrome::kChromeUIPrintHost;
}

void PrintPreviewDialogController::EraseInitiatorInfo(
    WebContents* preview_dialog) {
  auto it = preview_dialog_map_.find(preview_dialog);
  if (it == preview_dialog_map_.end())
    return;

  web_contents_collection_.StopObserving(it->second.initiator);
  it->second.initiator = nullptr;
  it->second.request_params = {};
  it->second.scoper.reset();
}

PrintPreviewDialogController::~PrintPreviewDialogController() = default;

PrintPreviewDialogController::InitiatorData::InitiatorData(
    InitiatorData&&) noexcept = default;

PrintPreviewDialogController::InitiatorData&
PrintPreviewDialogController::InitiatorData::operator=(
    InitiatorData&&) noexcept = default;

PrintPreviewDialogController::InitiatorData::InitiatorData(
    content::WebContents* initiator,
    const mojom::RequestPrintPreviewParams& request_params,
    std::unique_ptr<tabs::ScopedTabModalUI> scoper)
    : initiator(initiator),
      request_params(request_params),
      scoper(std::move(scoper)) {}

PrintPreviewDialogController::InitiatorData::~InitiatorData() = default;

void PrintPreviewDialogController::RenderProcessGone(
    content::WebContents* web_contents,
    base::TerminationStatus status) {
  content::RenderProcessHost* rph =
      web_contents->GetPrimaryMainFrame()->GetProcess();

  // Store contents in a vector and deal with them after iterating through
  // `preview_dialog_map_` because RemoveFoo() can change `preview_dialog_map_`.
  std::vector<WebContents*> closed_initiators;
  std::vector<WebContents*> closed_preview_dialogs;
  for (auto& it : preview_dialog_map_) {
    WebContents* preview_dialog = it.first;
    WebContents* initiator = it.second.initiator;
    if (preview_dialog->GetPrimaryMainFrame()->GetProcess() == rph)
      closed_preview_dialogs.push_back(preview_dialog);
    else if (initiator && initiator->GetPrimaryMainFrame()->GetProcess() == rph)
      closed_initiators.push_back(initiator);
  }

  for (WebContents* dialog : closed_preview_dialogs) {
    RemovePreviewDialog(dialog);
    auto* print_preview_ui = GetPrintPreviewUIForDialog(dialog);
    if (print_preview_ui)
      print_preview_ui->OnPrintPreviewDialogClosed();
  }

  for (WebContents* initiator : closed_initiators)
    RemoveInitiator(initiator);
}

void PrintPreviewDialogController::WebContentsDestroyed(WebContents* contents) {
  WebContents* preview_dialog = GetPrintPreviewForContents(contents);
  if (!preview_dialog) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  if (contents == preview_dialog)
    RemovePreviewDialog(contents);
  else
    RemoveInitiator(contents);
}

void PrintPreviewDialogController::DidFinishNavigation(
    WebContents* contents,
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  WebContents* preview_dialog = GetPrintPreviewForContents(contents);
  if (!preview_dialog) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  if (contents != preview_dialog)
    OnInitiatorNavigated(contents, navigation_handle);
  else
    OnPreviewDialogNavigated(contents, navigation_handle);
}

void PrintPreviewDialogController::OnInitiatorNavigated(
    WebContents* initiator,
    NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument()) {
    static const ui::PageTransition kTransitions[] = {
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
        ui::PAGE_TRANSITION_LINK, ui::PAGE_TRANSITION_AUTO_BOOKMARK};
    ui::PageTransition type =
        initiator->GetController().GetLastCommittedEntry()->GetTransitionType();
    for (ui::PageTransition transition : kTransitions) {
      if (ui::PageTransitionTypeIncludingQualifiersIs(type, transition))
        return;
    }
  }

  RemoveInitiator(initiator);
}

void PrintPreviewDialogController::OnPreviewDialogNavigated(
    WebContents* preview_dialog,
    NavigationHandle* navigation_handle) {
  ui::PageTransition type = preview_dialog->GetController()
                                .GetLastCommittedEntry()
                                ->GetTransitionType();

  // New `preview_dialog` is created. Don't update/erase map entry.
  if (navigation_handle->GetPreviousPrimaryMainFrameURL().is_empty() &&
      IsPrintPreviewURL(navigation_handle->GetURL()) &&
      ui::PageTransitionCoreTypeIs(type, ui::PAGE_TRANSITION_AUTO_TOPLEVEL) &&
      !navigation_handle->IsSameDocument()) {
    SaveInitiatorTitle(preview_dialog);
  }
}

WebContents* PrintPreviewDialogController::CreatePrintPreviewDialog(
    tabs::TabInterface* tab,
    content::WebContents* initiator,
    const mojom::RequestPrintPreviewParams& params) {
  base::AutoReset<bool> auto_reset(&is_creating_print_preview_dialog_, true);

  // The dialog delegates are deleted when the dialog is closed.
  ConstrainedWebDialogDelegate* web_dialog_delegate = ShowConstrainedWebDialog(
      initiator->GetBrowserContext(),
      std::make_unique<PrintPreviewDialogDelegate>(initiator), initiator);

  WebContents* preview_dialog = web_dialog_delegate->GetWebContents();

  // Clear the zoom level for the print preview dialog so it isn't affected by
  // the default zoom level. This also controls the zoom level of the OOP PDF
  // extension when iframed by the print preview dialog.
  GURL print_url(chrome::kChromeUIPrintURL);
  content::HostZoomMap::Get(preview_dialog->GetSiteInstance())
      ->SetZoomLevelForHostAndScheme(print_url.scheme(), print_url.host(), 0);
  PrintViewManager::CreateForWebContents(preview_dialog);

  // Add an entry to the map.
  InitiatorData data(initiator, params, tab ? tab->ShowModalUI() : nullptr);
  preview_dialog_map_.emplace(preview_dialog, std::move(data));

  // Make the print preview WebContents show up in the task manager.
  task_manager::WebContentsTags::CreateForPrintingContents(preview_dialog);

  web_contents_collection_.StartObserving(initiator);
  web_contents_collection_.StartObserving(preview_dialog);

  return preview_dialog;
}

void PrintPreviewDialogController::SaveInitiatorTitle(
    WebContents* preview_dialog) {
  WebContents* initiator = GetInitiator(preview_dialog);
  if (!initiator)
    return;

  auto* print_preview_ui = GetPrintPreviewUIForDialog(preview_dialog);
  if (!print_preview_ui)
    return;

  print_preview_ui->SetInitiatorTitle(
      PrintViewManager::FromWebContents(initiator)->RenderSourceName());
}

void PrintPreviewDialogController::RemoveInitiator(
    WebContents* initiator) {
  WebContents* preview_dialog = GetPrintPreviewForContents(initiator);
  DCHECK(preview_dialog);
  // Update the map entry first, so when the print preview dialog gets destroyed
  // and reaches RemovePreviewDialog(), it does not attempt to also remove the
  // initiator's observers.
  EraseInitiatorInfo(preview_dialog);

  PrintViewManager::FromWebContents(initiator)->PrintPreviewDone();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  CloseArcPrintSession(initiator);
#endif

  // Initiator is closed. Close the print preview dialog too.
  auto* print_preview_ui = GetPrintPreviewUIForDialog(preview_dialog);
  if (print_preview_ui)
    print_preview_ui->OnInitiatorClosed();
}

void PrintPreviewDialogController::RemovePreviewDialog(
    WebContents* preview_dialog) {
  // Remove the initiator's observers before erasing the mapping.
  WebContents* initiator = GetInitiator(preview_dialog);
  if (initiator) {
    web_contents_collection_.StopObserving(initiator);
    PrintViewManager::FromWebContents(initiator)->PrintPreviewDone();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    CloseArcPrintSession(initiator);
#endif
  }

  preview_dialog_map_.erase(preview_dialog);
  web_contents_collection_.StopObserving(preview_dialog);
}

}  // namespace printing
