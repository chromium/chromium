// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_FILE_UPLOAD_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_FILE_UPLOAD_TOOL_REQUEST_H_

#include <iosfwd>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "components/actor/core/shared_types.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

class ToolRequestVisitorFunctor;

// Represents a source file to be uploaded. Designed to be extensible for
// additional upload sources in future iterations.
struct FileUploadSource {
  enum class Type {
    kLocalPath,
  };

  Type type = Type::kLocalPath;
  base::FilePath local_path;
};

// Tool request to upload one or more files to a web page by interacting with
// a file input or triggering a file picker dialog.
class FileUploadToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "FileUpload";

  FileUploadToolRequest(tabs::TabHandle tab_handle,
                        PageTarget target,
                        std::vector<FileUploadSource> files);
  FileUploadToolRequest(const FileUploadToolRequest&);
  FileUploadToolRequest& operator=(const FileUploadToolRequest&);
  ~FileUploadToolRequest() override;

  // ToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  std::string_view Name() const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;

  const PageTarget& target() const { return target_; }
  const std::vector<FileUploadSource>& files() const { return files_; }

 private:
  PageTarget target_;
  std::vector<FileUploadSource> files_;
};

std::ostream& operator<<(std::ostream& out,
                         const FileUploadSource& file_source);
std::ostream& operator<<(std::ostream& out,
                         const std::vector<FileUploadSource>& file_sources);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_FILE_UPLOAD_TOOL_REQUEST_H_
