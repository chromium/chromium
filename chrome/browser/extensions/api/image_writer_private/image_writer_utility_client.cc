// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/image_writer_utility_client.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/optional.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/removable_storage_writer/public/mojom/constants.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {
namespace image_writer {

namespace {
ImageWriterUtilityClient::ImageWriterUtilityClientFactory*
    g_factory_for_testing = nullptr;

void DeleteInterfacePtr(chrome::mojom::RemovableStorageWriterPtr writer_ptr) {
  // Just let the parameters go out of scope so they are deleted.
}
}  // namespace

class ImageWriterUtilityClient::RemovableStorageWriterClientImpl
    : public chrome::mojom::RemovableStorageWriterClient {
 public:
  RemovableStorageWriterClientImpl(
      ImageWriterUtilityClient* owner,
      chrome::mojom::RemovableStorageWriterClientPtr* interface_ptr)
      : binding_(this, mojo::MakeRequest(interface_ptr)),
        image_writer_utility_client_(owner) {
    binding_.set_connection_error_handler(
        base::BindOnce(&ImageWriterUtilityClient::OnConnectionError,
                       image_writer_utility_client_));
  }

  ~RemovableStorageWriterClientImpl() override = default;

 private:
  void Progress(int64_t progress) override {
    image_writer_utility_client_->OperationProgress(progress);
  }

  void Complete(const base::Optional<std::string>& error) override {
    if (error) {
      image_writer_utility_client_->OperationFailed(error.value());
    } else {
      image_writer_utility_client_->OperationSucceeded();
    }
  }

  mojo::Binding<chrome::mojom::RemovableStorageWriterClient> binding_;
  // |image_writer_utility_client_| owns |this|.
  ImageWriterUtilityClient* const image_writer_utility_client_;

  DISALLOW_COPY_AND_ASSIGN(RemovableStorageWriterClientImpl);
};

ImageWriterUtilityClient::ImageWriterUtilityClient(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    std::unique_ptr<service_manager::Connector> connector)
    : task_runner_(task_runner), connector_(std::move(connector)) {}

ImageWriterUtilityClient::~ImageWriterUtilityClient() {
  // We could be running on a different TaskRunner (typically, the UI thread).
  // Post to be safe.
  task_runner_->DeleteSoon(FROM_HERE, std::move(connector_));
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&DeleteInterfacePtr,
                                        std::move(removable_storage_writer_)));
}

// static
scoped_refptr<ImageWriterUtilityClient> ImageWriterUtilityClient::Create(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    std::unique_ptr<service_manager::Connector> connector) {
  // connector_ can be null in unit-tests.
  DCHECK(!connector || !connector->IsBound());
  if (g_factory_for_testing)
    return g_factory_for_testing->Run();
  return base::WrapRefCounted(
      new ImageWriterUtilityClient(task_runner, std::move(connector)));
}

// static
void ImageWriterUtilityClient::SetFactoryForTesting(
    ImageWriterUtilityClientFactory* factory) {
  g_factory_for_testing = factory;
}

void ImageWriterUtilityClient::Write(const ProgressCallback& progress_callback,
                                     const SuccessCallback& success_callback,
                                     const ErrorCallback& error_callback,
                                     const base::FilePath& source,
                                     const base::FilePath& target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!removable_storage_writer_client_);

  progress_callback_ = progress_callback;
  success_callback_ = success_callback;
  error_callback_ = error_callback;

  BindServiceIfNeeded();

  chrome::mojom::RemovableStorageWriterClientPtr client;
  removable_storage_writer_client_ =
      std::make_unique<RemovableStorageWriterClientImpl>(this, &client);

  removable_storage_writer_->Write(source, target, std::move(client));
}

void ImageWriterUtilityClient::Verify(const ProgressCallback& progress_callback,
                                      const SuccessCallback& success_callback,
                                      const ErrorCallback& error_callback,
                                      const base::FilePath& source,
                                      const base::FilePath& target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!removable_storage_writer_client_);

  progress_callback_ = progress_callback;
  success_callback_ = success_callback;
  error_callback_ = error_callback;

  BindServiceIfNeeded();

  chrome::mojom::RemovableStorageWriterClientPtr client;
  removable_storage_writer_client_ =
      std::make_unique<RemovableStorageWriterClientImpl>(this, &client);

  removable_storage_writer_->Verify(source, target, std::move(client));
}

void ImageWriterUtilityClient::Cancel(const CancelCallback& cancel_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(cancel_callback);

  ResetRequest();
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, cancel_callback);
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

  connector_->BindInterface(chrome::mojom::kRemovableStorageWriterServiceName,
                            mojo::MakeRequest(&removable_storage_writer_));
  removable_storage_writer_.set_connection_error_handler(
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
  SuccessCallback success_callback = success_callback_;
  ResetRequest();
  if (success_callback)
    success_callback.Run();
}

void ImageWriterUtilityClient::OperationFailed(const std::string& error) {
  ErrorCallback error_callback = error_callback_;
  ResetRequest();
  if (error_callback)
    error_callback.Run(error);
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
