// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_IME_IME_SERVICE_H_
#define ASH_SERVICES_IME_IME_SERVICE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/services/ime/decoder/decoder_engine.h"
#include "ash/services/ime/input_engine.h"
#include "ash/services/ime/public/cpp/shared_lib/interfaces.h"
#include "ash/services/ime/public/mojom/ime_service.mojom.h"
#include "base/files/file_path.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace ime {

class ImeService : public mojom::ImeService,
                   public mojom::InputEngineManager,
                   public ImeCrosPlatform {
 public:
  explicit ImeService(mojo::PendingReceiver<mojom::ImeService> receiver);

  ImeService(const ImeService&) = delete;
  ImeService& operator=(const ImeService&) = delete;

  ~ImeService() override;

 private:
  // mojom::ImeService overrides:
  void SetPlatformAccessProvider(
      mojo::PendingRemote<mojom::PlatformAccessProvider> provider) override;
  void BindInputEngineManager(
      mojo::PendingReceiver<mojom::InputEngineManager> receiver) override;

  // mojom::InputEngineManager overrides:
  void ConnectToImeEngine(
      const std::string& ime_spec,
      mojo::PendingReceiver<mojom::InputChannel> to_engine_request,
      mojo::PendingRemote<mojom::InputChannel> from_engine,
      const std::vector<uint8_t>& extra,
      ConnectToImeEngineCallback callback) override;
  void ConnectToInputMethod(
      const std::string& ime_spec,
      mojo::PendingReceiver<mojom::InputMethod> input_method,
      mojo::PendingRemote<mojom::InputMethodHost> input_method_host,
      ConnectToInputMethodCallback callback) override;

  // ImeCrosPlatform overrides:
  const char* GetImeBundleDir() override;
  const char* GetImeUserHomeDir() override;
  // To be deprecated soon. Do not make a call on it anymore.
  const char* GetImeGlobalDir() override;

  int SimpleDownloadToFile(const char* url,
                           const char* file_path,
                           SimpleDownloadCallback callback) override;
  int SimpleDownloadToFileV2(const char* url,
                             const char* file_path,
                             SimpleDownloadCallbackV2 callback) override;
  ImeCrosDownloader* GetDownloader() override;
  void RunInMainSequence(ImeSequencedTask task, int task_id) override;
  bool IsFeatureEnabled(const char* feature_name) override;

  // Callback used when a file download finishes by the |SimpleURLLoader|.
  // On failure, |file| will be empty.
  void SimpleDownloadFinished(SimpleDownloadCallback callback,
                              const base::FilePath& file);
  // V2 of |SimpleDownloadFinished|, returns an extra URL with |file|.
  // Callback used when a file download finishes by the |SimpleURLLoader|.
  // The |url| is the original download url and bound when downloading request
  // starts. On failure, |file| will be empty.
  void SimpleDownloadFinishedV2(SimpleDownloadCallbackV2 callback,
                                const std::string& url_str,
                                const base::FilePath& file);
  const MojoSystemThunks* GetMojoSystemThunks() override;

  mojo::Receiver<mojom::ImeService> receiver_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // For the duration of this service lifetime, there should be only one
  // decoder engine or input engine instance.
  std::unique_ptr<DecoderEngine> decoder_engine_;
  std::unique_ptr<InputEngine> input_engine_;

  // Platform delegate for access to privilege resources.
  mojo::Remote<mojom::PlatformAccessProvider> platform_access_;
  mojo::ReceiverSet<mojom::InputEngineManager> manager_receivers_;
};

}  // namespace ime
}  // namespace chromeos

#endif  // ASH_SERVICES_IME_IME_SERVICE_H_
