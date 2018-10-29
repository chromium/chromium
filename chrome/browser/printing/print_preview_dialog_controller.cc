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
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
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
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

using content::NavigationController;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

// A ui::WebDialogDelegate that specifies the print preview dialog appearance.
class PrintPreviewDialogDelegate : public ui::WebDialogDelegate {
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
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(WebContents* source, bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;

 private:
  WebContents* initiator_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewDialogDelegate);
};

PrintPreviewDialogDelegate::PrintPreviewDialogDelegate(WebContents* initiator)
    : initiator_(initiator) {
}

PrintPreviewDialogDelegate::~PrintPreviewDialogDelegate() {
}

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
      guest_view::GuestViewBase::GetTopLevelWebContents(initiator_);
  Browser* browser = chrome::FindBrowserWithWebContents(outermost_web_contents);
  if (browser)
    host = browser->window()->GetWebContentsModalDialogHost();

  if (host)
    size->SetToMax(host->GetMaximumDialogSize());
  else
    size->SetToMax(outermost_web_contents->GetContainerBounds().size());
  size->Enlarge(-2 * kBorder, -kBorder);

#if defined(OS_MACOSX)
  // Limit the maximum size on MacOS X.
  // http://crbug.com/105815
  const gfx::Size kMaxDialogSize(1000, 660);
  size->SetToMin(kMaxDialogSize);
#endif
}

std::string PrintPreviewDialogDelegate::GetDialogArgs() const {
  return std::string();
}

void PrintPreviewDialogDelegate::OnDialogClosed(
    const std::string& /* json_retval */) {
}

void PrintPreviewDialogDelegate::OnCloseContents(WebContents* /* source */,
                                                 bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool PrintPreviewDialogDelegate::ShouldShowDialogTitle() const {
  return false;
}

}  // namespace

namespace printing {

PrintPreviewDialogController::PrintPreviewDialogController()
    : waiting_for_new_preview_page_(false),
      is_creating_print_preview_dialog_(false) {
}

// static
PrintPreviewDialogController* PrintPreviewDialogController::GetInstance() {
  if (!g_browser_process)
    return nullptr;
  return g_browser_process->print_preview_dialog_controller();
}

// static
void PrintPreviewDialogController::PrintPreview(WebContents* initiator) {
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

void PrintPreviewDialogController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_RENDERER_PROCESS_CLOSED) {
    OnRendererProcessClosed(
        content::Source<content::RenderProcessHost>(source).ptr());
  } else if (type == content::NOTIFICATION_WEB_CONTENTS_DESTROYED) {
    OnWebContentsDestroyed(content::Source<WebContents>(source).ptr());
  } else {
    DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);
    WebContents* contents =
        content::Source<NavigationController>(source)->GetWebContents();
    OnNavEntryCommitted(
        contents,
        content::Details<content::LoadCommittedDetails>(details).ptr());
  }
}

void PrintPreviewDialogController::ForEachPreviewDialog(
    base::Callback<void(content::WebContents*)> callback) {
  for (PrintPreviewDialogMap::const_iterator it = preview_dialog_map_.begin();
       it != preview_dialog_map_.end();
       ++it) {
    callback.Run(it->first);
  }
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

  RemoveObservers(it->second);
  preview_dialog_map_[preview_dialog] = nullptr;
}

PrintPreviewDialogController::~PrintPreviewDialogController() {}

void PrintPreviewDialogController::OnRendererProcessClosed(
    content::RenderProcessHost* rph) {
  // Store contents in a vector and deal with them after iterating through
  // |preview_dialog_map_| because RemoveFoo() can change |preview_dialog_map_|.
  std::vector<WebContents*> closed_initiators;
  std::vector<WebContents*> closed_preview_dialogs;
  for (auto iter = preview_dialog_map_.begin();
       iter != preview_dialog_map_.end(); ++iter) {
    WebContents* preview_dialog = iter->first;
    WebContents* initiator = iter->second;
    if (preview_dialog->GetMainFrame()->GetProcess() == rph) {
      closed_preview_dialogs.push_back(preview_dialog);
    } else if (initiator && initiator->GetMainFrame()->GetProcess() == rph) {
      closed_initiators.push_back(initiator);
    }
  }

  for (size_t i = 0; i < closed_preview_dialogs.size(); ++i) {
    RemovePreviewDialog(closed_preview_dialogs[i]);
    if (content::WebUI* web_ui = closed_preview_dialogs[i]->GetWebUI()) {
      PrintPreviewUI* print_preview_ui =
          static_cast<PrintPreviewUI*>(web_ui->GetController());
      if (print_preview_ui)
        print_preview_ui->OnPrintPreviewDialogClosed();
    }
  }

  for (size_t i = 0; i < closed_initiators.size(); ++i)
    RemoveInitiator(closed_initiators[i]);
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
    WebContents* contents, content::LoadCommittedDetails* details) {
  WebContents* preview_dialog = GetPrintPreviewForContents(contents);
  if (!preview_dialog) {
    NOTREACHED();
    return;
  }

  if (contents == preview_dialog) {
    // Preview dialog navigated.
    if (details) {
      ui::PageTransition transition_type =
          details->entry->GetTransitionType();
      content::NavigationType nav_type = details->type;

      // New |preview_dialog| is created. Don't update/erase map entry.
      if (waiting_for_new_preview_page_ &&
          ui::PageTransitionCoreTypeIs(transition_type,
                                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL) &&
          nav_type == content::NAVIGATION_TYPE_NEW_PAGE) {
        waiting_for_new_preview_page_ = false;
        SaveInitiatorTitle(preview_dialog);
        return;
      }

      // Cloud print sign-in causes a reload.
      if (!waiting_for_new_preview_page_ &&
          ui::PageTransitionCoreTypeIs(transition_type,
                                       ui::PAGE_TRANSITION_RELOAD) &&
          nav_type == content::NAVIGATION_TYPE_EXISTING_PAGE &&
          IsPrintPreviewURL(details->previous_url)) {
        return;
      }
    }
    NOTREACHED();
    return;
  }
  if (details) {
    ui::PageTransition type = details->entry->GetTransitionType();
    content::NavigationType nav_type = details->type;
    if (nav_type == content::NAVIGATION_TYPE_EXISTING_PAGE &&
        (ui::PageTransitionTypeIncludingQualifiersIs(
             type,
             ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                       ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)) ||
         ui::PageTransitionTypeIncludingQualifiersIs(type,
                                                     ui::PAGE_TRANSITION_LINK)))
      return;
  }

  RemoveInitiator(contents);
}

WebContents* PrintPreviewDialogController::CreatePrintPreviewDialog(
    WebContents* initiator) {
  base::AutoReset<bool> auto_reset(&is_creating_print_preview_dialog_, true);

  // The dialog delegates are deleted when the dialog is closed.
  ConstrainedWebDialogDelegate* web_dialog_delegate =
      ShowConstrainedWebDialog(initiator->GetBrowserContext(),
                               new PrintPreviewDialogDelegate(initiator),
                               initiator);

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

  AddObservers(initiator);
  AddObservers(preview_dialog);

  return preview_dialog;
}

void PrintPreviewDialogController::SaveInitiatorTitle(
    WebContents* preview_dialog) {
  WebContents* initiator = GetInitiator(preview_dialog);
  if (initiator && preview_dialog->GetWebUI()) {
    PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
        preview_dialog->GetWebUI()->GetController());
    print_preview_ui->SetInitiatorTitle(
        PrintViewManager::FromWebContents(initiator)->RenderSourceName());
  }
}

void PrintPreviewDialogController::AddObservers(WebContents* contents) {
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<WebContents>(contents));
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&contents->GetController()));

  // Multiple sites may share the same RenderProcessHost, so check if this
  // notification has already been added.
  content::Source<content::RenderProcessHost> rph_source(
      contents->GetMainFrame()->GetProcess());
  if (!registrar_.IsRegistered(this,
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED, rph_source)) {
    // Not registered for this host yet, so add the notification and add the
    // host to the count map with a count of 1.
    registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                   rph_source);
    host_contents_count_map_[contents->GetMainFrame()->GetProcess()] = 1;
  } else {
    // This host's notification is already registered. Increment its count in
    // the map so that the notification will not be removed from the registry
    // until all web contents that use it are destroyed.
    ++host_contents_count_map_[contents->GetMainFrame()->GetProcess()];
  }
}

void PrintPreviewDialogController::RemoveObservers(WebContents* contents) {
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                    content::Source<WebContents>(contents));
  registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&contents->GetController()));

  // Multiple sites may share the same RenderProcessHost, so check if this
  // notification has already been added.
  content::Source<content::RenderProcessHost> rph_source(
      contents->GetMainFrame()->GetProcess());
  if (registrar_.IsRegistered(this,
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED, rph_source)) {
    if (host_contents_count_map_[contents->GetMainFrame()->GetProcess()] == 1) {
      // This is the last contents that has this render process host, so we can
      // remove the notification.
      registrar_.Remove(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                        rph_source);
      host_contents_count_map_.erase(contents->GetMainFrame()->GetProcess());
    } else {
      // Other initializers and/or dialogs are still connected to the host, so
      // we can't remove the notification. Decrement the count in the map.
      --host_contents_count_map_[contents->GetMainFrame()->GetProcess()];
    }
  }
}

void PrintPreviewDialogController::RemoveInitiator(
    WebContents* initiator) {
  WebContents* preview_dialog = GetPrintPreviewForContents(initiator);
  DCHECK(preview_dialog);
  // Update the map entry first, so when the print preview dialog gets destroyed
  // and reaches RemovePreviewDialog(), it does not attempt to also remove the
  // initiator's observers.
  preview_dialog_map_[preview_dialog] = nullptr;
  RemoveObservers(initiator);

  PrintViewManager::FromWebContents(initiator)->PrintPreviewDone();

  // initiator is closed. Close the print preview dialog too.
  if (content::WebUI* web_ui = preview_dialog->GetWebUI()) {
    PrintPreviewUI* print_preview_ui =
        static_cast<PrintPreviewUI*>(web_ui->GetController());
    if (print_preview_ui)
      print_preview_ui->OnInitiatorClosed();
  }
}

void PrintPreviewDialogController::RemovePreviewDialog(
    WebContents* preview_dialog) {
  // Remove the initiator's observers before erasing the mapping.
  WebContents* initiator = GetInitiator(preview_dialog);
  if (initiator) {
    RemoveObservers(initiator);
    PrintViewManager::FromWebContents(initiator)->PrintPreviewDone();
  }

  preview_dialog_map_.erase(preview_dialog);
  RemoveObservers(preview_dialog);
}

}  // namespace printing
