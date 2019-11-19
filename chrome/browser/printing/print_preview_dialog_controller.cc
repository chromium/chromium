// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_dialog_controller.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/win/conflicts/module_database.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/print_spooler/print_session_impl.h"
#endif

using content::NavigationController;
using content::WebContents;
using content::WebUIMessageHandler;

namespace printing {

namespace {

PrintPreviewUI* GetPrintPreviewUIForDialog(WebContents* dialog) {
  content::WebUI* web_ui = dialog->GetWebUI();
  return web_ui ? static_cast<PrintPreviewUI*>(web_ui->GetController())
                : nullptr;
}

#if defined(OS_CHROMEOS)
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
class PrintPreviewDialogDelegate : public ui::WebDialogDelegate,
                                   public content::WebContentsObserver {
 public:
  explicit PrintPreviewDialogDelegate(WebContents* initiator);
  ~PrintPreviewDialogDelegate() override;

  ui::ModalType GetDialogModalType() const override;
  base::string16 GetDialogTitle() const override;
  base::string16 GetAccessibleDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogClosingFromKeyEvent() override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(WebContents* source, bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;

 private:
  WebContents* initiator() const { return web_contents(); }

  bool on_dialog_closed_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewDialogDelegate);
};

PrintPreviewDialogDelegate::PrintPreviewDialogDelegate(WebContents* initiator)
    : content::WebContentsObserver(initiator) {}

PrintPreviewDialogDelegate::~PrintPreviewDialogDelegate() = default;

ui::ModalType PrintPreviewDialogDelegate::GetDialogModalType() const {
  // Not used, returning dummy value.
  NOTREACHED();
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 PrintPreviewDialogDelegate::GetDialogTitle() const {
  // Only used on Windows? UI folks prefer no title.
  return base::string16();
}

base::string16 PrintPreviewDialogDelegate::GetAccessibleDialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_PRINT_PREVIEW_TITLE);
}

GURL PrintPreviewDialogDelegate::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIPrintURL);
}

void PrintPreviewDialogDelegate::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* /* handlers */) const {
  // PrintPreviewUI adds its own message handlers.
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

  Browser* browser = chrome::FindBrowserWithWebContents(outermost_web_contents);
  if (browser)
    host = browser->window()->GetWebContentsModalDialogHost();

  if (host)
    size->SetToMax(host->GetMaximumDialogSize());
  else
    size->SetToMax(outermost_web_contents->GetContainerBounds().size());
  size->Enlarge(-2 * kBorder, -kBorder);

  static const gfx::Size kMaxDialogSize(1000, 660);
  size->SetToMin(kMaxDialogSize);
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

// PrintPreviewDialogController::WebContentsObserver ---------------------------

class PrintPreviewDialogController::WebContentsObserver
    : public content::WebContentsObserver {
 public:
  WebContentsObserver(PrintPreviewDialogController* controller,
                      content::WebContents* web_contents);
  ~WebContentsObserver() override;

  // content::WebContentsObserver:
  void RenderProcessGone(base::TerminationStatus status) override;
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void WebContentsDestroyed() override;

 private:
  PrintPreviewDialogController* const controller_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsObserver);
};

PrintPreviewDialogController::WebContentsObserver::WebContentsObserver(
    PrintPreviewDialogController* controller,
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), controller_(controller) {}

PrintPreviewDialogController::WebContentsObserver::~WebContentsObserver() =
    default;

void PrintPreviewDialogController::WebContentsObserver::RenderProcessGone(
    base::TerminationStatus status) {
  controller_->OnRendererProcessClosed(
      web_contents()->GetMainFrame()->GetProcess());
}

void PrintPreviewDialogController::WebContentsObserver::
    NavigationEntryCommitted(
        const content::LoadCommittedDetails& load_details) {
  controller_->OnNavEntryCommitted(web_contents(), load_details);
}

void PrintPreviewDialogController::WebContentsObserver::WebContentsDestroyed() {
  controller_->OnWebContentsDestroyed(web_contents());
}

// PrintPreviewDialogController ------------------------------------------------

PrintPreviewDialogController::PrintPreviewDialogController() = default;

// static
PrintPreviewDialogController* PrintPreviewDialogController::GetInstance() {
  if (!g_browser_process)
    return nullptr;
  return g_browser_process->print_preview_dialog_controller();
}

// static
void PrintPreviewDialogController::PrintPreview(WebContents* initiator) {
#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  ModuleDatabase::GetInstance()->DisableThirdPartyBlocking();
#endif

  if (initiator->ShowingInterstitialPage() || initiator->IsCrashed())
    return;

  PrintPreviewDialogController* dialog_controller = GetInstance();
  if (!dialog_controller)
    return;
  if (!dialog_controller->GetOrCreatePreviewDialog(initiator)) {
    PrintViewManager* print_view_manager =
        PrintViewManager::FromWebContents(initiator);
    if (print_view_manager)
      print_view_manager->PrintPreviewDone();
  }
}

WebContents* PrintPreviewDialogController::GetOrCreatePreviewDialog(
    WebContents* initiator) {
  DCHECK(initiator);

  // Get the print preview dialog for |initiator|.
  WebContents* preview_dialog = GetPrintPreviewForContents(initiator);
  if (!preview_dialog)
    return CreatePrintPreviewDialog(initiator);

  // Show the initiator holding the existing preview dialog.
  initiator->GetDelegate()->ActivateContents(initiator);
  return preview_dialog;
}

WebContents* PrintPreviewDialogController::GetPrintPreviewForContents(
    WebContents* contents) const {
  // |preview_dialog_map_| is keyed by the preview dialog, so if find()
  // succeeds, then |contents| is the preview dialog.
  auto it = preview_dialog_map_.find(contents);
  if (it != preview_dialog_map_.end())
    return contents;

  for (it = preview_dialog_map_.begin();
       it != preview_dialog_map_.end();
       ++it) {
    // If |contents| is an initiator.
    if (contents == it->second) {
      // Return the associated preview dialog.
      return it->first;
    }
  }
  return nullptr;
}

WebContents* PrintPreviewDialogController::GetInitiator(
    WebContents* preview_dialog) {
  auto it = preview_dialog_map_.find(preview_dialog);
  return (it != preview_dialog_map_.end()) ? it->second : nullptr;
}

void PrintPreviewDialogController::ForEachPreviewDialog(
    base::RepeatingCallback<void(content::WebContents*)> callback) {
  for (const auto& it : preview_dialog_map_)
    callback.Run(it.first);
}

// static
bool PrintPreviewDialogController::IsPrintPreviewURL(const GURL& url) {
  return (url.SchemeIs(content::kChromeUIScheme) &&
          url.host_piece() == chrome::kChromeUIPrintHost);
}

void PrintPreviewDialogController::EraseInitiatorInfo(
    WebContents* preview_dialog) {
  auto it = preview_dialog_map_.find(preview_dialog);
  if (it == preview_dialog_map_.end())
    return;

  RemoveObserver(it->second);
  preview_dialog_map_[preview_dialog] = nullptr;
}

PrintPreviewDialogController::~PrintPreviewDialogController() = default;

void PrintPreviewDialogController::OnRendererProcessClosed(
    content::RenderProcessHost* rph) {
  // Store contents in a vector and deal with them after iterating through
  // |preview_dialog_map_| because RemoveFoo() can change |preview_dialog_map_|.
  std::vector<WebContents*> closed_initiators;
  std::vector<WebContents*> closed_preview_dialogs;
  for (auto& it : preview_dialog_map_) {
    WebContents* preview_dialog = it.first;
    WebContents* initiator = it.second;
    if (preview_dialog->GetMainFrame()->GetProcess() == rph)
      closed_preview_dialogs.push_back(preview_dialog);
    else if (initiator && initiator->GetMainFrame()->GetProcess() == rph)
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

void PrintPreviewDialogController::OnWebContentsDestroyed(
    WebContents* contents) {
  WebContents* preview_dialog = GetPrintPreviewForContents(contents);
  if (!preview_dialog) {
    NOTREACHED();
    return;
  }

  if (contents == preview_dialog)
    RemovePreviewDialog(contents);
  else
    RemoveInitiator(contents);
}

void PrintPreviewDialogController::OnNavEntryCommitted(
    WebContents* contents,
    const content::LoadCommittedDetails& details) {
  WebContents* preview_dialog = GetPrintPreviewForContents(contents);
  if (!preview_dialog) {
    NOTREACHED();
    return;
  }

  if (contents != preview_dialog)
    OnInitiatorNavigated(contents, details);
  else
    OnPreviewDialogNavigated(contents, details);
}

void PrintPreviewDialogController::OnInitiatorNavigated(
    WebContents* initiator,
    const content::LoadCommittedDetails& details) {
  if (details.type == content::NAVIGATION_TYPE_EXISTING_PAGE) {
    static const ui::PageTransition kTransitions[] = {
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
        ui::PAGE_TRANSITION_LINK,
    };
    ui::PageTransition type = details.entry->GetTransitionType();
    for (ui::PageTransition transition : kTransitions) {
      if (ui::PageTransitionTypeIncludingQualifiersIs(type, transition))
        return;
    }
  }

  RemoveInitiator(initiator);
}

void PrintPreviewDialogController::OnPreviewDialogNavigated(
    WebContents* preview_dialog,
    const content::LoadCommittedDetails& details) {
  ui::PageTransition type = details.entry->GetTransitionType();

  // New |preview_dialog| is created. Don't update/erase map entry.
  if (waiting_for_new_preview_page_ &&
      ui::PageTransitionCoreTypeIs(type, ui::PAGE_TRANSITION_AUTO_TOPLEVEL) &&
      details.type == content::NAVIGATION_TYPE_NEW_PAGE) {
    waiting_for_new_preview_page_ = false;
    SaveInitiatorTitle(preview_dialog);
    return;
  }

  // Cloud print sign-in causes a reload.
  if (!waiting_for_new_preview_page_ &&
      ui::PageTransitionCoreTypeIs(type, ui::PAGE_TRANSITION_RELOAD) &&
      details.type == content::NAVIGATION_TYPE_EXISTING_PAGE &&
      IsPrintPreviewURL(details.previous_url)) {
    return;
  }

  NOTREACHED();
}

WebContents* PrintPreviewDialogController::CreatePrintPreviewDialog(
    WebContents* initiator) {
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
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      preview_dialog);

  // Add an entry to the map.
  preview_dialog_map_[preview_dialog] = initiator;
  waiting_for_new_preview_page_ = true;

  // Make the print preview WebContents show up in the task manager.
  task_manager::WebContentsTags::CreateForPrintingContents(preview_dialog);

  AddObserver(initiator);
  AddObserver(preview_dialog);

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

void PrintPreviewDialogController::AddObserver(WebContents* contents) {
  auto emplace_result = web_contents_observers_.emplace(
      std::piecewise_construct, std::forward_as_tuple(contents),
      std::forward_as_tuple(
          std::make_unique<WebContentsObserver>(this, contents)));
  DCHECK(emplace_result.second);
}

void PrintPreviewDialogController::RemoveObserver(WebContents* contents) {
  web_contents_observers_.erase(contents);
}

void PrintPreviewDialogController::RemoveInitiator(
    WebContents* initiator) {
  WebContents* preview_dialog = GetPrintPreviewForContents(initiator);
  DCHECK(preview_dialog);
  // Update the map entry first, so when the print preview dialog gets destroyed
  // and reaches RemovePreviewDialog(), it does not attempt to also remove the
  // initiator's observers.
  preview_dialog_map_[preview_dialog] = nullptr;
  RemoveObserver(initiator);

  PrintViewManager::FromWebContents(initiator)->PrintPreviewDone();

#if defined(OS_CHROMEOS)
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
    RemoveObserver(initiator);
    PrintViewManager::FromWebContents(initiator)->PrintPreviewDone();

#if defined(OS_CHROMEOS)
    CloseArcPrintSession(initiator);
#endif
  }

  preview_dialog_map_.erase(preview_dialog);
  RemoveObserver(preview_dialog);
}

}  // namespace printing
