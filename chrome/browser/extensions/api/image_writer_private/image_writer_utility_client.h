// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_UTILITY_CLIENT_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_UTILITY_CLIENT_H_

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/services/removable_storage_writer/public/mojom/removable_storage_writer.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace extensions {
namespace image_writer {

// Writes a disk image to a device inside the utility process. This
// class lives on the FILE thread.
class ImageWriterUtilityClient
    : public base::RefCountedThreadSafe<ImageWriterUtilityClient> {
 public:
  using CancelCallback = base::OnceClosure;
  using SuccessCallback = base::OnceClosure;
  using ProgressCallback = base::RepeatingCallback<void(int64_t)>;
  using ErrorCallback = base::OnceCallback<void(const std::string&)>;
  using ImageWriterUtilityClientFactory =
      base::RepeatingCallback<scoped_refptr<ImageWriterUtilityClient>()>;

  ImageWriterUtilityClient(const ImageWriterUtilityClient&) = delete;
  ImageWriterUtilityClient& operator=(const ImageWriterUtilityClient&) = delete;

  static scoped_refptr<ImageWriterUtilityClient> Create(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  static void SetFactoryForTesting(ImageWriterUtilityClientFactory* factory);

  // Starts the write operation.
  // |progress_callback|: Called periodically with the count of bytes processed.
  // |success_callback|: Called at successful completion.
  // |error_callback|: Called with an error message on failure.
  // |source|: The path to the source file to read data from.
  // |target|: The path to the target device to write the image file to.
  virtual void Write(ProgressCallback progress_callback,
                     SuccessCallback success_callback,
                     ErrorCallback error_callback,
                     const base::FilePath& source,
                     const base::FilePath& target);

  // Starts a verify operation.
  // |progress_callback|: Called periodically with the count of bytes processed.
  // |success_callback|: Called at successful completion.
  // |error_callback|: Called with an error message on failure.
  // |source|: The path to the source file to read data from.
  // |target|: The path to the target device file to verify.
  virtual void Verify(ProgressCallback progress_callback,
                      SuccessCallback success_callback,
                      ErrorCallback error_callback,
                      const base::FilePath& source,
                      const base::FilePath& target);

  // Cancels any pending write or verify operation.
  // |cancel_callback|: Called when the cancel has actually occurred.
  // TODO(crbug.com/40511721): Consider removing this API.
  virtual void Cancel(CancelCallback cancel_callback);

  // Shuts down the utility process that may have been created.
  virtual void Shutdown();

 protected:
  friend class base::RefCountedThreadSafe<ImageWriterUtilityClient>;
  friend class ImageWriterUtilityClientTest;

  explicit ImageWriterUtilityClient(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  virtual ~ImageWriterUtilityClient();

 private:
  class RemovableStorageWriterClientImpl;

  void BindServiceIfNeeded();
  void OnConnectionError();

  void OperationProgress(int64_t progress);
  void OperationSucceeded();
  void OperationFailed(const std::string& error);

  void ResetRequest();

  ProgressCallback progress_callback_;
  SuccessCallback success_callback_;
  ErrorCallback error_callback_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  mojo::Remote<chrome::mojom::RemovableStorageWriter> removable_storage_writer_;

  std::unique_ptr<RemovableStorageWriterClientImpl>
      removable_storage_writer_client_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_IMAGE_WRITER_UTILITY_CLIENT_H_
