// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/image_writer_utility_client.h"

#include <optional>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/removable_storage_writer/public/mojom/removable_storage_writer.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {
namespace image_writer {

namespace {

ImageWriterUtilityClient::ImageWriterUtilityClientFactory*
    g_factory_for_testing = nullptr;

void DeleteRemote(mojo::Remote<chrome::mojom::RemovableStorageWriter> writer) {
  // Just let the parameter go out of scope so it's deleted.
}

}  // namespace

class ImageWriterUtilityClient::RemovableStorageWriterClientImpl
    : public chrome::mojom::RemovableStorageWriterClient {
 public:
  RemovableStorageWriterClientImpl(
      ImageWriterUtilityClient* owner,
      mojo::PendingReceiver<chrome::mojom::RemovableStorageWriterClient>
          receiver)
      : receiver_(this, std::move(receiver)),
        image_writer_utility_client_(owner) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&ImageWriterUtilityClient::OnConnectionError,
                       image_writer_utility_client_));
  }

  RemovableStorageWriterClientImpl(const RemovableStorageWriterClientImpl&) =
      delete;
  RemovableStorageWriterClientImpl& operator=(
      const RemovableStorageWriterClientImpl&) = delete;

  ~RemovableStorageWriterClientImpl() override = default;

 private:
  void Progress(int64_t progress) override {
    image_writer_utility_client_->OperationProgress(progress);
  }

  void Complete(const std::optional<std::string>& error) override {
    if (error) {
      image_writer_utility_client_->OperationFailed(error.value());
    } else {
      image_writer_utility_client_->OperationSucceeded();
    }
  }

  mojo::Receiver<chrome::mojom::RemovableStorageWriterClient> receiver_;

  // |image_writer_utility_client_| owns |this|.
  const raw_ptr<ImageWriterUtilityClient> image_writer_utility_client_;
};

ImageWriterUtilityClient::ImageWriterUtilityClient(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner) {}

ImageWriterUtilityClient::~ImageWriterUtilityClient() {
  // We could be running on a different TaskRunner (typically, the UI thread).
  // Post to be safe.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteRemote, std::move(removable_storage_writer_)));
}

// static
scoped_refptr<ImageWriterUtilityClient> ImageWriterUtilityClient::Create(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  if (g_factory_for_testing)
    return g_factory_for_testing->Run();
  return base::WrapRefCounted(new ImageWriterUtilityClient(task_runner));
}

// static
void ImageWriterUtilityClient::SetFactoryForTesting(
    ImageWriterUtilityClientFactory* factory) {
  g_factory_for_testing = factory;
}

void ImageWriterUtilityClient::Write(ProgressCallback progress_callback,
                                     SuccessCallback success_callback,
                                     ErrorCallback error_callback,
                                     const base::FilePath& source,
                                     const base::FilePath& target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!removable_storage_writer_client_);

  progress_callback_ = std::move(progress_callback);
  success_callback_ = std::move(success_callback);
  error_callback_ = std::move(error_callback);

  BindServiceIfNeeded();

  mojo::PendingRemote<chrome::mojom::RemovableStorageWriterClient>
      remote_client;
  removable_storage_writer_client_ =
      std::make_unique<RemovableStorageWriterClientImpl>(
          this, remote_client.InitWithNewPipeAndPassReceiver());
  removable_storage_writer_->Write(source, target, std::move(remote_client));
}

void ImageWriterUtilityClient::Verify(ProgressCallback progress_callback,
                                      SuccessCallback success_callback,
                                      ErrorCallback error_callback,
                                      const base::FilePath& source,
                                      const base::FilePath& target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!removable_storage_writer_client_);

  progress_callback_ = std::move(progress_callback);
  success_callback_ = std::move(success_callback);
  error_callback_ = std::move(error_callback);

  BindServiceIfNeeded();

  mojo::PendingRemote<chrome::mojom::RemovableStorageWriterClient>
      remote_client;
  removable_storage_writer_client_ =
      std::make_unique<RemovableStorageWriterClientImpl>(
          this, remote_client.InitWithNewPipeAndPassReceiver());
  removable_storage_writer_->Verify(source, target, std::move(remote_client));
}

void ImageWriterUtilityClient::Cancel(CancelCallback cancel_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(cancel_callback);

  ResetRequest();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(cancel_callback));
}

void ImageWriterUtilityClient::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ResetRequest();
  removable_storage_writer_.reset();
}

void ImageWriterUtilityClient::BindServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (removable_storage_writer_)
    return;

  content::ServiceProcessHost::Launch(
      removable_storage_writer_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_UTILITY_PROCESS_IMAGE_WRITER_NAME)
          .Pass());
  removable_storage_writer_.set_disconnect_handler(
      base::BindOnce(&ImageWriterUtilityClient::OnConnectionError, this));
}

void ImageWriterUtilityClient::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  OperationFailed(
      "Error with the connection to the RemovableStorageWriter service.");
  removable_storage_writer_.reset();
}

void ImageWriterUtilityClient::OperationProgress(int64_t progress) {
  if (progress_callback_)
    progress_callback_.Run(progress);
}

void ImageWriterUtilityClient::OperationSucceeded() {
  SuccessCallback success_callback = std::move(success_callback_);
  ResetRequest();
  if (success_callback)
    std::move(success_callback).Run();
}

void ImageWriterUtilityClient::OperationFailed(const std::string& error) {
  ErrorCallback error_callback = std::move(error_callback_);
  ResetRequest();
  if (error_callback)
    std::move(error_callback).Run(error);
}

void ImageWriterUtilityClient::ResetRequest() {
  removable_storage_writer_client_.reset();

  // Clear handlers to not hold any reference to the caller.
  progress_callback_.Reset();
  success_callback_.Reset();
  error_callback_.Reset();
}

}  // namespace image_writer
}  // namespace extensions
