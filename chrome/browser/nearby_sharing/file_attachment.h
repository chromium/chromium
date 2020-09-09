// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_FILE_ATTACHMENT_H_
#define CHROME_BROWSER_NEARBY_SHARING_FILE_ATTACHMENT_H_

#include <string>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "chrome/browser/nearby_sharing/attachment.h"
#include "chrome/services/sharing/public/mojom/nearby_decoder_types.mojom.h"

// A single attachment to be sent by / received from a |ShareTarget|, can be
// either a file or text.
class FileAttachment : public Attachment {
 public:
  using Type = sharing::mojom::FileMetadata::Type;

  explicit FileAttachment(base::FilePath file_path);
  FileAttachment(int64_t id,
                 int64_t size,
                 std::string file_name,
                 std::string mime_type,
                 Type type);
  FileAttachment(const FileAttachment&);
  FileAttachment(FileAttachment&&);
  FileAttachment& operator=(const FileAttachment&);
  FileAttachment& operator=(FileAttachment&&);
  ~FileAttachment() override;

  const std::string& file_name() const { return file_name_; }
  const std::string& mime_type() const { return mime_type_; }
  Type type() const { return type_; }
  const base::Optional<base::FilePath>& file_path() const { return file_path_; }

  // Attachment:
  void MoveToShareTarget(ShareTarget& share_target) override;
  const std::string& GetDescription() const override;

  void set_file_path(base::Optional<base::FilePath> path) {
    file_path_ = std::move(path);
  }

 private:
  std::string file_name_;
  std::string mime_type_;
  Type type_;
  base::Optional<base::FilePath> file_path_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_FILE_ATTACHMENT_H_
