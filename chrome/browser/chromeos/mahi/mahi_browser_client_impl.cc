// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/mahi_browser_client_impl.h"

#include <string>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/mahi/mahi_browser_util.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_content_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_util.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "ui/gfx/image/image_skia.h"
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#else  // BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/check_deref.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/mahi/mahi_browser_delegate_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace mahi {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
crosapi::mojom::MahiBrowserDelegate* g_mahi_browser_delegate_for_testing =
    nullptr;

// Adds a helper function so that the `MahiBrowserDelegate` can be overridden in
// the test.
crosapi::mojom::MahiBrowserDelegate& mahi_browser_delegate() {
  if (g_mahi_browser_delegate_for_testing) {
    return CHECK_DEREF(g_mahi_browser_delegate_for_testing);
  }
  return CHECK_DEREF(crosapi::CrosapiManager::Get()
                         ->crosapi_ash()
                         ->mahi_browser_delegate_ash());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

MahiBrowserClientImpl::MahiBrowserClientImpl(
    base::RepeatingCallback<void(const base::UnguessableToken&,
                                 GetContentCallback)> request_content_callback)
    : client_id_(base::UnguessableToken::Create()),
      request_content_callback_(request_content_callback) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Bind remote and pass receiver to `MahiBrowserDelegate`.
  chromeos::LacrosService::Get()->BindMahiBrowserDelegate(
      remote_.BindNewPipeAndPassReceiver());
  // Register the mojo client.
  remote_->RegisterMojoClient(receiver_.BindNewPipeAndPassRemote(), client_id_,
                              base::BindOnce([](bool success) {
                                if (!success) {
                                  LOG(ERROR)
                                      << "MahiBrowserClientImpl "
                                         "RegisterMojoClient did not succeed.";
                                }
                              }));
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  // Register the C++ (non-mojo) client.
  if (crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->mahi_browser_delegate_ash()
        ->RegisterCppClient(this, client_id_);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

MahiBrowserClientImpl::~MahiBrowserClientImpl() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // C++ clients are responsible for manually calling `UnregisterClient()` on
  // the manager when disconnecting.
  if (crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->mahi_browser_delegate_ash()
        ->UnregisterClient(client_id_);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void MahiBrowserClientImpl::OnFocusedPageChanged(
    const WebContentState& web_content_state) {
  // Generates `page_info` from `web_content_state`.
  crosapi::mojom::MahiPageInfoPtr page_info = crosapi::mojom::MahiPageInfo::New(
      /*client_id=*/client_id(),
      /*page_id=*/web_content_state.page_id,
      /*url=*/web_content_state.url, /*title=*/web_content_state.title,
      /*favicon_image=*/web_content_state.favicon.DeepCopy(),
      /*is_distillable=*/std::nullopt,
      /*is_incognito=*/web_content_state.is_incognito);
  if (web_content_state.is_distillable.has_value()) {
    page_info->IsDistillable = web_content_state.is_distillable.value();
  }

  auto callback = base::BindOnce([](bool success) {
    if (!success) {
      LOG(ERROR) << "MahiBrowser::OnFocusedPageChanged did not succeed.";
    }
  });

// Sends page info to `MahiBrowserDelegate`.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  remote_->OnFocusedPageChanged(std::move(page_info), std::move(callback));
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  // Do not notify browser delegate if the top level native window is observed
  // by media app content provider (i.e. the web_content is from a media app
  // window), to avoid overriding media app focus status.
  CHECK(chromeos::MahiMediaAppContentManager::Get());
  if (chromeos::MahiMediaAppContentManager::Get()->ObservingWindow(
          web_content_state.top_level_native_window)) {
    return;
  }

  mahi_browser_delegate().OnFocusedPageChanged(std::move(page_info),
                                               std::move(callback));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

void MahiBrowserClientImpl::OnContextMenuClicked(
    int64_t display_id,
    chromeos::mahi::ButtonType button_type,
    const std::u16string& question,
    const gfx::Rect& mahi_menu_bounds) {
  // Generates the context menu request.
  crosapi::mojom::MahiContextMenuRequestPtr context_menu_request =
      crosapi::mojom::MahiContextMenuRequest::New(
          /*display_id=*/display_id,
          /*action_type=*/
          chromeos::mahi::MatchButtonTypeToActionType(button_type),
          /*question=*/std::nullopt,
          /*mahi_menu_bounds=*/mahi_menu_bounds);
  if (button_type == chromeos::mahi::ButtonType::kQA) {
    context_menu_request->question = question;
  }

  auto callback = base::BindOnce(
      [](chromeos::mahi::ButtonType button_type, bool success) {
        if (!success) {
          // Records the `button_type` clicking did not succeed.
          base::UmaHistogramEnumeration(
              chromeos::mahi::kMahiContextMenuActivatedFailed, button_type);
          LOG(ERROR) << "MahiBrowser::OnContextMenuClicked did not succeed.";
        }
      },
      button_type);

// Sends request to `MahiBrowserDelegate`.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  remote_->OnContextMenuClicked(std::move(context_menu_request),
                                std::move(callback));
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  mahi_browser_delegate().OnContextMenuClicked(std::move(context_menu_request),
                                               std::move(callback));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

void MahiBrowserClientImpl::GetContent(const base::UnguessableToken& page_id,
                                       GetContentCallback callback) {
  // Forwards the request to `MahiWebContentManager`.
  request_content_callback_.Run(std::move(page_id), std::move(callback));
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void MahiBrowserClientImpl::BindMahiBrowserDelegateForTesting(
    mojo::PendingRemote<crosapi::mojom::MahiBrowserDelegate> pending_remote) {
  remote_.reset();
  remote_.Bind(std::move(pending_remote));
}
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
void MahiBrowserClientImpl::SetMahiBrowserDelegateForTesting(
    crosapi::mojom::MahiBrowserDelegate* delegate) {
  g_mahi_browser_delegate_for_testing = delegate;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace mahi
