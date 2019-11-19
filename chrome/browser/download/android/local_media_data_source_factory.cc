// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/local_media_data_source_factory.h"

#include <vector>

#include "base/android/content_uri_utils.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace {

using MediaDataCallback =
    SafeMediaMetadataParser::MediaDataSourceFactory::MediaDataCallback;
using ReadFileCallback = base::OnceCallback<void(bool, std::vector<char>)>;

// Helper method to post |cb| on the |main_task_runner| with read result.
void OnReadComplete(scoped_refptr<base::SequencedTaskRunner> main_task_runner,
                    ReadFileCallback cb,
                    bool success,
                    std::vector<char> data) {
  main_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(cb), success, std::move(data)));
}

// Reads a chunk of the file on a file thread, and reply the data or error to
// main thread.
void ReadFile(const base::FilePath& file_path,
              int64_t position,
              int64_t length,
              scoped_refptr<base::SequencedTaskRunner> main_task_runner,
              ReadFileCallback cb) {
  base::File file;
#if defined(OS_ANDROID)
  if (file_path.IsContentUri()) {
    file = base::OpenContentUriForRead(file_path);
    if (!file.IsValid()) {
      OnReadComplete(main_task_runner, std::move(cb), false /*success*/,
                     std::vector<char>());
      return;
    }
  }
#endif  // defined(OS_ANDROID)
  if (!file.IsValid())
    file = base::File(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    OnReadComplete(main_task_runner, std::move(cb), false /*success*/,
                   std::vector<char>());
    return;
  }

  auto buffer = std::vector<char>(length);
  int bytes_read = file.Read(position, buffer.data(), length);
  if (bytes_read == -1) {
    OnReadComplete(main_task_runner, std::move(cb), false /*success*/,
                   std::vector<char>());
    return;
  }
  DCHECK_GE(bytes_read, 0);
  if (bytes_read < length)
    buffer.resize(bytes_read);

  OnReadComplete(main_task_runner, std::move(cb), true /*success*/,
                 std::move(buffer));
}

// Read media file incrementally and send data to the utility process to parse
// media metadata. Must live and die on main thread and does blocking IO on
// |file_task_runner_|.
class LocalMediaDataSource : public chrome::mojom::MediaDataSource {
 public:
  LocalMediaDataSource(
      mojo::PendingReceiver<chrome::mojom::MediaDataSource> receiver,
      const base::FilePath& file_path,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner,
      MediaDataCallback media_data_callback)
      : file_path_(file_path),
        file_task_runner_(file_task_runner),
        media_data_callback_(media_data_callback),
        receiver_(this, std::move(receiver)) {}
  ~LocalMediaDataSource() override = default;

 private:
  // chrome::mojom::MediaDataSource implementation.
  void Read(int64_t position,
            int64_t length,
            chrome::mojom::MediaDataSource::ReadCallback callback) override {
    DCHECK(!ipc_read_callback_);
    ipc_read_callback_ = std::move(callback);

    // Read file on a file thread.
    ReadFileCallback read_file_done = base::BindOnce(
        &LocalMediaDataSource::OnReadFileDone, weak_ptr_factory_.GetWeakPtr());
    file_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ReadFile, file_path_, position, length,
                                  base::ThreadTaskRunnerHandle::Get(),
                                  std::move(read_file_done)));
  }

  void OnReadFileDone(bool success, std::vector<char> buffer) {
    // TODO(xingliu): Handle file IO error when success is false, the IPC
    // channel for chrome::mojom::MediaParser should be closed.
    DCHECK(ipc_read_callback_);
    media_data_callback_.Run(
        std::move(ipc_read_callback_),
        std::make_unique<std::string>(buffer.begin(), buffer.end()));
  }

  base::FilePath file_path_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Called when a chunk of the file is read.
  MediaDataCallback media_data_callback_;

  // Pass through callback that is used to send data across IPC channel.
  chrome::mojom::MediaDataSource::ReadCallback ipc_read_callback_;

  mojo::Receiver<chrome::mojom::MediaDataSource> receiver_;
  base::WeakPtrFactory<LocalMediaDataSource> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LocalMediaDataSource);
};

}  // namespace

LocalMediaDataSourceFactory::LocalMediaDataSourceFactory(
    const base::FilePath& file_path,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : file_path_(file_path), file_task_runner_(file_task_runner) {}

LocalMediaDataSourceFactory::~LocalMediaDataSourceFactory() = default;

std::unique_ptr<chrome::mojom::MediaDataSource>
LocalMediaDataSourceFactory::CreateMediaDataSource(
    mojo::PendingReceiver<chrome::mojom::MediaDataSource> receiver,
    MediaDataCallback media_data_callback) {
  return std::make_unique<LocalMediaDataSource>(
      std::move(receiver), file_path_, file_task_runner_, media_data_callback);
}
