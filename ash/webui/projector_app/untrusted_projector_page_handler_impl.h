// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_UNTRUSTED_PROJECTOR_PAGE_HANDLER_IMPL_H_
#define ASH_WEBUI_PROJECTOR_APP_UNTRUSTED_PROJECTOR_PAGE_HANDLER_IMPL_H_

#include "ash/webui/projector_app/mojom/untrusted_projector.mojom.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/projector_xhr_sender.h"
#include "ash/webui/projector_app/public/mojom/projector_types.mojom-forward.h"
#include "base/files/safe_base_name.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace ash {

// Handles messages from the Projector WebUIs (i.e.
// chrome-untrusted://projector).
class UntrustedProjectorPageHandlerImpl
    : public ProjectorAppClient::Observer,
      public projector::mojom::UntrustedProjectorPageHandler {
 public:
  UntrustedProjectorPageHandlerImpl(
      mojo::PendingReceiver<projector::mojom::UntrustedProjectorPageHandler>
          receiver,
      mojo::PendingRemote<projector::mojom::UntrustedProjectorPage>
          projector_remote,
      PrefService* pref_service);
  UntrustedProjectorPageHandlerImpl(const UntrustedProjectorPageHandlerImpl&) =
      delete;
  UntrustedProjectorPageHandlerImpl& operator=(
      const UntrustedProjectorPageHandlerImpl&) = delete;
  ~UntrustedProjectorPageHandlerImpl() override;

  // ProjectorAppClient:Observer:
  void OnNewScreencastPreconditionChanged(
      const NewScreencastPrecondition& precondition) override;
  void OnSodaProgress(int percentage) override;
  void OnSodaError() override;
  void OnSodaInstalled() override;
  void OnScreencastsPendingStatusChanged(
      const PendingScreencastContainerSet& pending_screencast) override;

  //  projector::mojom::UntrustedProjectorPageHandler:
  void GetNewScreencastPrecondition(
      GetNewScreencastPreconditionCallback callback) override;
  void ShouldDownloadSoda(ShouldDownloadSodaCallback callback) override;
  void InstallSoda(InstallSodaCallback callback) override;
  void GetPendingScreencasts(GetPendingScreencastsCallback callback) override;
  void GetUserPref(projector::mojom::PrefsThatProjectorCanAskFor pref,
                   GetUserPrefCallback callback) override;
  void SetUserPref(projector::mojom::PrefsThatProjectorCanAskFor pref,
                   base::Value value,
                   SetUserPrefCallback callback) override;
  void OpenFeedbackDialog(OpenFeedbackDialogCallback callback) override;
  void StartProjectorSession(const base::SafeBaseName& storage_dir_name,
                             StartProjectorSessionCallback callback) override;
  void SendXhr(
      const GURL& url,
      projector::mojom::RequestType method,
      const std::optional<std::string>& request_body,
      bool use_credentials,
      bool use_api_key,
      const std::optional<base::flat_map<std::string, std::string>>& headers,
      const std::optional<std::string>& account_email,
      SendXhrCallback callback) override;
  void GetAccounts(GetAccountsCallback callback) override;
  void GetVideo(const std::string& video_file_id,
                const std::optional<std::string>& resource_key,
                GetVideoCallback callback) override;

 protected:
  void OnVideoLocated(GetVideoCallback callback,
                      projector::mojom::GetVideoResultPtr result);

  base::WeakPtr<UntrustedProjectorPageHandlerImpl> GetWeakPtr();

  // Called when the XHR request is completed. Runs the callback with the
  // results.
  virtual void OnXhrRequestCompleted(
      SendXhrCallback callback,
      projector::mojom::XhrResponsePtr xhr_response);

 private:
  mojo::Receiver<projector::mojom::UntrustedProjectorPageHandler> receiver_;
  mojo::Remote<projector::mojom::UntrustedProjectorPage> projector_remote_;

  // Primary user pref service.
  const raw_ptr<PrefService> pref_service_;
  ProjectorXhrSender xhr_sender_;

  base::WeakPtrFactory<UntrustedProjectorPageHandlerImpl> weak_ptr_factory_{
      this};
};

}  // namespace ash
#endif  // ASH_WEBUI_PROJECTOR_APP_UNTRUSTED_PROJECTOR_PAGE_HANDLER_IMPL_H_
