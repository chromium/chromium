// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_file_system_instance.h"

#include <string.h>
#include <unistd.h>

#include <limits>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

namespace {

mojom::DocumentPtr MakeDocument(const FakeFileSystemInstance::Document& doc) {
  mojom::DocumentPtr document = mojom::Document::New();
  document->document_id = doc.document_id;
  document->display_name = doc.display_name;
  document->mime_type = doc.mime_type;
  document->size = doc.size;
  document->last_modified = doc.last_modified;
  document->supports_delete = doc.supports_delete;
  document->supports_rename = doc.supports_rename;
  document->dir_supports_create = doc.dir_supports_create;
  document->supports_thumbnail = doc.supports_thumbnail;
  return document;
}

mojom::RootPtr MakeRoot(const FakeFileSystemInstance::Root& in_root) {
  mojom::RootPtr root = mojom::Root::New();
  root->authority = in_root.authority;
  root->root_id = in_root.root_id;
  root->document_id = in_root.document_id;
  root->title = in_root.title;
  return root;
}

// Generates unique document ID on each call.
std::string GenerateDocumentId() {
  static int count = 0;
  std::ostringstream ss;
  ss << "doc_" << count++;
  return ss.str();
}

// Generates unique URL ID on each call.
std::string GenerateUrlId() {
  static int count = 0;
  std::ostringstream ss;
  ss << "url_" << count++;
  return ss.str();
}

// Maximum size in bytes to read FD from PIPE.
constexpr size_t kMaxBytesToReadFromPipe = 8 * 1024;  // 8KB;

}  // namespace

constexpr base::FilePath::CharType FakeFileSystemInstance::kFakeAndroidPath[];

constexpr gfx::Size FakeFileSystemInstance::kDefaultThumbnailSize;

FakeFileSystemInstance::File::File(const std::string& url,
                                   const std::string& content,
                                   const std::string& mime_type,
                                   Seekable seekable)
    : url(url), content(content), mime_type(mime_type), seekable(seekable) {}

FakeFileSystemInstance::File::File(const std::string& url,
                                   const std::string& content,
                                   const std::string& mime_type,
                                   Seekable seekable,
                                   int64_t size_override)
    : url(url),
      content(content),
      mime_type(mime_type),
      seekable(seekable),
      size_override(size_override) {}

FakeFileSystemInstance::File::File(const File& that) = default;

FakeFileSystemInstance::File::~File() = default;

FakeFileSystemInstance::Document::Document(
    const std::string& authority,
    const std::string& document_id,
    const std::string& parent_document_id,
    const std::string& display_name,
    const std::string& mime_type,
    int64_t size,
    uint64_t last_modified)
    : Document(authority,
               document_id,
               parent_document_id,
               display_name,
               mime_type,
               size,
               last_modified,
               true,
               true,
               true,
               false) {}

FakeFileSystemInstance::Document::Document(
    const std::string& authority,
    const std::string& document_id,
    const std::string& parent_document_id,
    const std::string& display_name,
    const std::string& mime_type,
    int64_t size,
    uint64_t last_modified,
    bool supports_delete,
    bool supports_rename,
    bool dir_supports_create,
    bool supports_thumbnail)
    : authority(authority),
      document_id(document_id),
      parent_document_id(parent_document_id),
      display_name(display_name),
      mime_type(mime_type),
      size(size),
      last_modified(last_modified),
      supports_delete(supports_delete),
      supports_rename(supports_rename),
      dir_supports_create(dir_supports_create),
      supports_thumbnail(supports_thumbnail) {}

FakeFileSystemInstance::Document::Document(const Document& that) = default;

FakeFileSystemInstance::Document::~Document() = default;

FakeFileSystemInstance::Root::Root(const std::string& authority,
                                   const std::string& root_id,
                                   const std::string& document_id,
                                   const std::string& title,
                                   int64_t available_bytes,
                                   int64_t capacity_bytes)
    : authority(authority),
      root_id(root_id),
      document_id(document_id),
      title(title),
      available_bytes(available_bytes),
      capacity_bytes(capacity_bytes) {}

FakeFileSystemInstance::Root::Root(const Root& that) = default;

FakeFileSystemInstance::Root::~Root() = default;

FakeFileSystemInstance::FakeFileSystemInstance() {
  bool temp_dir_created = temp_dir_.CreateUniqueTempDir();
  DCHECK(temp_dir_created);
}

FakeFileSystemInstance::~FakeFileSystemInstance() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

bool FakeFileSystemInstance::InitCalled() {
  return host_remote_.is_bound();
}

void FakeFileSystemInstance::AddFile(const File& file) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(0u, files_.count(std::string(file.url)));
  files_.insert(std::make_pair(std::string(file.url), file));
}

void FakeFileSystemInstance::AddDocument(const Document& document) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DocumentKey key(document.authority, document.document_id);
  DCHECK_EQ(0u, documents_.count(key));
  documents_.insert(std::make_pair(key, document));
  child_documents_[key];  // Allocate a vector.
  if (!document.parent_document_id.empty()) {
    DocumentKey parent_key(document.authority, document.parent_document_id);
    DCHECK_EQ(1u, documents_.count(parent_key));
    child_documents_[parent_key].push_back(key);
  }
}

void FakeFileSystemInstance::AddRecentDocument(const std::string& root_id,
                                               const Document& document) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RootKey key(document.authority, root_id);
  recent_documents_[key].push_back(document);
}

void FakeFileSystemInstance::RemoveRecentDocument(const Document& document) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Unfortunately we don't know the root_id when deleting a document, so
  // here we need to loop through all available roots to find the document.
  for (auto const& doc : recent_documents_) {
    const auto iter = base::ranges::find_if(
        doc.second, [&document](const Document& recent_document) {
          return document.authority == recent_document.authority &&
                 document.document_id == recent_document.document_id;
        });
    if (iter != doc.second.end()) {
      recent_documents_[doc.first].erase(iter);
      return;
    }
  }
}

void FakeFileSystemInstance::AddRoot(const Root& root) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  roots_list_.push_back(root);
  RootKey key(root.authority, root.root_id);
  DCHECK_EQ(0u, roots_.count(key));
  roots_.insert(std::make_pair(key, root));
}

void FakeFileSystemInstance::AddOpenSession(const std::string& url_id,
                                            const int fd) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(0u, open_urls_.count(url_id));
  open_urls_.insert(std::make_pair(url_id, fd));
}

void FakeFileSystemInstance::SetGetLastChangeTimeCallback(
    GetLastChangeTimeCallback ctime_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ctime_callback_ = ctime_callback;
}

void FakeFileSystemInstance::SetCrosDir(const base::FilePath& cros_dir) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  cros_dir_ = cros_dir;
}

void FakeFileSystemInstance::SetMediaStore(
    const std::map<base::FilePath, base::Time>& media_store) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  media_store_ = media_store;
}

void FakeFileSystemInstance::TriggerWatchers(
    const std::string& authority,
    const std::string& document_id,
    storage::WatcherManager::ChangeType type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!host_remote_) {
    LOG(ERROR) << "FileSystemHost is not available.";
    return;
  }
  auto iter = document_to_watchers_.find(DocumentKey(authority, document_id));
  if (iter == document_to_watchers_.end())
    return;
  for (int64_t watcher_id : iter->second) {
    host_remote_->OnDocumentChanged(watcher_id, type);
  }
}

bool FakeFileSystemInstance::DocumentExists(const std::string& authority,
                                            const std::string& document_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DocumentKey key(authority, document_id);
  return base::Contains(documents_, key);
}

bool FakeFileSystemInstance::DocumentExists(const std::string& authority,
                                            const std::string& root_document_id,
                                            const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<std::string> path_components = path.GetComponents();
  std::string document_id =
      FindChildDocumentId(authority, root_document_id, path_components);
  return DocumentExists(authority, document_id);
}

bool FakeFileSystemInstance::RootExists(const std::string& authority,
                                        const std::string& root_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RootKey key(authority, root_id);
  return base::Contains(roots_, key);
}

FakeFileSystemInstance::Document FakeFileSystemInstance::GetDocument(
    const std::string& authority,
    const std::string& document_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DocumentKey key(authority, document_id);
  auto iter = documents_.find(key);
  DCHECK(iter != documents_.end());
  return iter->second;
}

FakeFileSystemInstance::Document FakeFileSystemInstance::GetDocument(
    const std::string& authority,
    const std::string& root_document_id,
    const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<std::string> path_components = path.GetComponents();
  std::string document_id =
      FindChildDocumentId(authority, root_document_id, path_components);
  return GetDocument(authority, document_id);
}

std::string FakeFileSystemInstance::GetFileContent(const std::string& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetFileContent(url, std::numeric_limits<size_t>::max());
}

std::string FakeFileSystemInstance::GetFileContent(const std::string& url,
                                                   size_t bytes) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto regular_file_paths_it = regular_file_paths_.find(url);
  if (regular_file_paths_it != regular_file_paths_.end()) {
    base::FilePath path = regular_file_paths_it->second;
    std::string content;
    if (base::ReadFileToStringWithMaxSize(path, &content, bytes))
      return content;
  } else {
    auto pipe_read_ends_it = pipe_read_ends_.find(url);
    if (pipe_read_ends_it != pipe_read_ends_.end()) {
      if (bytes > kMaxBytesToReadFromPipe) {
        LOG(ERROR) << "Trying to read too many bytes from pipe. " << url;
        return std::string();
      }
      std::string result;
      result.resize(bytes);
      bool success = base::ReadFromFD(pipe_read_ends_it->second.get(), result);
      DCHECK(success);
      return result;
    }
  }
  LOG(ERROR) << "A file to read content not found. " << url;
  return std::string();
}

void FakeFileSystemInstance::AddWatcher(const std::string& authority,
                                        const std::string& document_id,
                                        AddWatcherCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DocumentKey key(authority, document_id);
  auto iter = documents_.find(key);
  if (iter == documents_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), -1));
    return;
  }
  int64_t watcher_id = next_watcher_id_++;
  document_to_watchers_[key].insert(watcher_id);
  watcher_to_document_.insert(std::make_pair(watcher_id, key));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), watcher_id));
}

void FakeFileSystemInstance::GetFileSize(const std::string& url,
                                         GetFileSizeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto iter = files_.find(url);
  if (iter == files_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), -1));
    return;
  }
  const File& file = iter->second;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), file.size()));
}

void FakeFileSystemInstance::GetMimeType(const std::string& url,
                                         GetMimeTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto iter = files_.find(url);
  if (iter == files_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }
  const File& file = iter->second;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), file.mime_type));
}

void FakeFileSystemInstance::CloseFileSession(
    const std::string& url_id,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto iter = open_urls_.find(url_id);
  if (iter != open_urls_.end())
    return;
  open_urls_.erase(url_id);
}

void FakeFileSystemInstance::OpenFileSessionToWrite(
    const GURL& url,
    OpenFileSessionToWriteCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto iter = files_.find(url.spec());
  if (iter == files_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), mojom::FileSessionPtr()));
    return;
  }
  const File& file = iter->second;
  base::ScopedFD fd =
      file.seekable == File::Seekable::YES
          ? CreateRegularFileDescriptor(file, base::File::Flags::FLAG_OPEN |
                                                  base::File::Flags::FLAG_WRITE)
          : CreateStreamFileDescriptorToWrite(file.url);
  DCHECK(fd.is_valid());
  std::string url_id = GenerateUrlId();
  AddOpenSession(url_id, fd.get());
  mojo::ScopedHandle wrapped_handle =
      mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(fd)));
  DCHECK(wrapped_handle.is_valid());
  mojom::FileSessionPtr file_session = mojom::FileSession::New();
  file_session->url_id = std::move(url_id);
  file_session->fd = std::move(wrapped_handle);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(file_session)));
}

void FakeFileSystemInstance::OpenFileSessionToRead(
    const GURL& url,
    OpenFileSessionToReadCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto iter = files_.find(url.spec());
  if (iter == files_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), mojom::FileSessionPtr()));
    return;
  }
  const File& file = iter->second;
  base::ScopedFD fd =
      file.seekable == File::Seekable::YES
          ? CreateRegularFileDescriptor(file, base::File::Flags::FLAG_OPEN |
                                                  base::File::Flags::FLAG_READ)
          : CreateStreamFileDescriptorToRead(file.content);
  DCHECK(fd.is_valid());
  std::string url_id = GenerateUrlId();
  AddOpenSession(url_id, fd.get());
  mojo::ScopedHandle wrapped_handle =
      mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(fd)));
  DCHECK(wrapped_handle.is_valid());
  mojom::FileSessionPtr file_session = mojom::FileSession::New();
  file_session->url_id = std::move(url_id);
  file_session->fd = std::move(wrapped_handle);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(file_session)));
}

void FakeFileSystemInstance::OpenThumbnail(const std::string& url,
                                           const gfx::Size& size_hint,
                                           OpenThumbnailCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto iter = files_.find(url);
  if (iter == files_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), mojo::ScopedHandle()));
    return;
  }
  const File& file = iter->second;
  if (file.thumbnail_content.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), mojo::ScopedHandle()));
    return;
  }
  // This validates that size_hint parameter is propagated properly from the
  // client, so OpenThumbnail should always be called with same default value in
  // tests.
  if (size_hint != kDefaultThumbnailSize) {
    LOG(ERROR) << "Unexpected thumbnail size hint: " << size_hint.width() << "x"
               << size_hint.height();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), mojo::ScopedHandle()));
    return;
  }
  base::ScopedFD fd = CreateStreamFileDescriptorToRead(file.thumbnail_content);
  mojo::ScopedHandle wrapped_handle =
      mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(fd)));
  DCHECK(wrapped_handle.is_valid());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(wrapped_handle)));
}

void FakeFileSystemInstance::GetDocument(const std::string& authority,
                                         const std::string& document_id,
                                         GetDocumentCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto iter = documents_.find(DocumentKey(authority, document_id));
  if (iter == documents_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), mojom::DocumentPtr()));
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), MakeDocument(iter->second)));
}

void FakeFileSystemInstance::GetChildDocuments(
    const std::string& authority,
    const std::string& parent_document_id,
    GetChildDocumentsCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ++get_child_documents_count_;
  auto child_iter =
      child_documents_.find(DocumentKey(authority, parent_document_id));
  if (child_iter == child_documents_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }
  std::vector<mojom::DocumentPtr> children;
  for (const auto& child_key : child_iter->second) {
    auto doc_iter = documents_.find(child_key);
    DCHECK(doc_iter != documents_.end());
    children.emplace_back(MakeDocument(doc_iter->second));
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                std::make_optional(std::move(children))));
}

void FakeFileSystemInstance::GetRecentDocuments(
    const std::string& authority,
    const std::string& root_id,
    GetRecentDocumentsCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto recent_iter = recent_documents_.find(RootKey(authority, root_id));
  if (recent_iter == recent_documents_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }
  std::vector<mojom::DocumentPtr> recents;
  for (const Document& document : recent_iter->second)
    recents.emplace_back(MakeDocument(document));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                std::make_optional(std::move(recents))));
}

void FakeFileSystemInstance::GetRoots(GetRootsCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<mojom::RootPtr> roots;
  for (const Root& root : roots_list_)
    roots.emplace_back(MakeRoot(root));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                std::make_optional(std::move(roots))));
}

void FakeFileSystemInstance::GetRootSize(const std::string& authority,
                                         const std::string& root_id,
                                         GetRootSizeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto iter = roots_.find(RootKey(authority, root_id));
  if (iter == roots_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), mojom::RootSizePtr()));
    return;
  }
  const Root& root = iter->second;
  mojom::RootSizePtr root_size = mojom::RootSize::New();
  root_size->available_bytes = root.available_bytes;
  root_size->capacity_bytes = root.capacity_bytes;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(root_size)));
}

void FakeFileSystemInstance::DeleteDocument(const std::string& authority,
                                            const std::string& document_id,
                                            DeleteDocumentCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DocumentKey key(authority, document_id);
  auto iter = documents_.find(key);
  if (iter == documents_.end() || iter->second.supports_delete == false) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  // We also need to remove the document from the recent_documents_ if it was
  // being added there.
  RemoveRecentDocument(iter->second);
  documents_.erase(iter);
  size_t erased = child_documents_.erase(key);
  DCHECK_NE(0u, erased);

  // Remove this document from lists of children.
  for (auto& child_iter : child_documents_) {
    std::erase(child_iter.second, key);
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeFileSystemInstance::RenameDocument(const std::string& authority,
                                            const std::string& document_id,
                                            const std::string& display_name,
                                            RenameDocumentCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DocumentKey key(authority, document_id);
  auto iter = documents_.find(key);
  if (iter == documents_.end() || iter->second.supports_rename == false) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), mojom::DocumentPtr()));
    return;
  }
  iter->second.display_name = display_name;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), MakeDocument(iter->second)));
}

void FakeFileSystemInstance::CreateDocument(
    const std::string& authority,
    const std::string& parent_document_id,
    const std::string& mime_type,
    const std::string& display_name,
    CreateDocumentCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DocumentKey parent_key(authority, parent_document_id);
  auto iter = documents_.find(parent_key);
  DCHECK(iter != documents_.end());
  if (iter->second.dir_supports_create == false) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), mojom::DocumentPtr()));
    return;
  }
  std::string document_id = GenerateDocumentId();
  Document document(authority, document_id, parent_document_id, display_name,
                    mime_type, 0, 0);
  AddDocument(document);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), MakeDocument(document)));
}

void FakeFileSystemInstance::CopyDocument(
    const std::string& authority,
    const std::string& source_document_id,
    const std::string& target_parent_document_id,
    CopyDocumentCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DocumentKey source_key(authority, source_document_id);
  auto iter = documents_.find(source_key);
  DCHECK(iter != documents_.end());
  Document target_document(iter->second);
  target_document.document_id = target_document.display_name;
  target_document.parent_document_id = target_parent_document_id;
  AddDocument(target_document);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), MakeDocument(target_document)));
}

void FakeFileSystemInstance::MoveDocument(
    const std::string& authority,
    const std::string& source_document_id,
    const std::string& source_parent_document_id,
    const std::string& target_parent_document_id,
    MoveDocumentCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DocumentKey source_key(authority, source_document_id);
  DocumentKey source_parent_key(authority, source_parent_document_id);
  DocumentKey target_parent_key(authority, target_parent_document_id);
  for (auto iter = child_documents_[source_parent_key].begin();
       iter != child_documents_[source_parent_key].end(); iter++) {
    if (*iter == source_key) {
      child_documents_[source_parent_key].erase(iter);
      break;
    }
  }
  child_documents_[target_parent_key].push_back(source_key);
  auto iter = documents_.find(source_key);
  DCHECK(iter != documents_.end());
  iter->second.parent_document_id = target_parent_document_id;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), MakeDocument(iter->second)));
}

void FakeFileSystemInstance::Init(
    mojo::PendingRemote<mojom::FileSystemHost> host_remote,
    InitCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(host_remote);
  DCHECK(!host_remote_);
  host_remote_.Bind(std::move(host_remote));
  std::move(callback).Run();
}

void FakeFileSystemInstance::RemoveWatcher(int64_t watcher_id,
                                           RemoveWatcherCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto iter = watcher_to_document_.find(watcher_id);
  if (iter == watcher_to_document_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  document_to_watchers_[iter->second].erase(watcher_id);
  watcher_to_document_.erase(iter);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

// TODO(risan): "Added" directory might not be handled. Please double check
// this.
void FakeFileSystemInstance::RequestMediaScan(
    const std::vector<std::string>& paths) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(risan): This is to prevent crashing other tests that expect nothing
  // from RequestMediaScan, e.g., the following:
  // FilesAppBrowserTest.Test/dirContextMenuDocumentsProvider_DocumentsProvider
  if (cros_dir_.empty())
    return;
  for (const auto& path : paths) {
    base::FilePath file_path = base::FilePath(path);
    base::FilePath cros_path = GetCrosPath(file_path);
    if (PathExists(cros_path)) {
      // For each existing path, index itself and all parent directories of
      // it.
      base::Time ctime;
      if (!DirectoryExists(cros_path))
        ctime = ctime_callback_.Run(cros_path);
      media_store_[file_path] = ctime;
      file_path = file_path.DirName();
      while (file_path != base::FilePath(kFakeAndroidPath).DirName()) {
        media_store_[file_path] = base::Time();
        file_path = file_path.DirName();
      }
    } else {
      // When a file or directory does not exist, it means it has been
      // deleted. So we need to erase its index entry in |media_store_|, and
      // also the entries of all files/directories underneath it if it is a
      // directory.
      for (auto it = media_store_.begin(); it != media_store_.end();) {
        if (it->first == file_path || file_path.IsParent(it->first))
          media_store_.erase(it++);
        else
          ++it;
      }
    }
  }
}

void FakeFileSystemInstance::RequestFileRemovalScan(
    const std::vector<std::string>& directory_paths) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ReindexDirectory(kFakeAndroidPath);
}

void FakeFileSystemInstance::ReindexDirectory(
    const std::string& directory_path) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<std::string> paths = {directory_path};
  base::FilePath directory_file_path(directory_path);
  for (const auto& entry : media_store_) {
    base::FilePath entry_path = entry.first;
    if (!directory_file_path.IsParent(entry_path)) {
      continue;
    }
    paths.push_back(entry_path.value());
  }
  RequestMediaScan(paths);
}

void FakeFileSystemInstance::DEPRECATED_OpenUrlsWithPermission(
    mojom::OpenUrlsRequestPtr request,
    DEPRECATED_OpenUrlsWithPermissionCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  handled_url_requests_.emplace_back(std::move(request));
}

void FakeFileSystemInstance::OpenUrlsWithPermissionAndWindowInfo(
    mojom::OpenUrlsRequestPtr request,
    mojom::WindowInfoPtr window_info,
    OpenUrlsWithPermissionAndWindowInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  handled_url_requests_.emplace_back(std::move(request));
}

std::string FakeFileSystemInstance::FindChildDocumentId(
    const std::string& authority,
    const std::string& parent_document_id,
    const std::vector<std::string>& components) {
  if (components.empty())
    return parent_document_id;

  auto children_iter =
      child_documents_.find(DocumentKey(authority, parent_document_id));
  if (children_iter == child_documents_.end())
    return std::string();

  for (DocumentKey key : children_iter->second) {
    auto iter = documents_.find(key);
    if (iter == documents_.end())
      continue;

    if (iter->second.display_name == components[0]) {
      std::vector<std::string> next_components(components.begin() + 1,
                                               components.end());
      return FindChildDocumentId(authority, iter->second.document_id,
                                 next_components);
    }
  }
  return std::string();
}

base::ScopedFD FakeFileSystemInstance::CreateRegularFileDescriptor(
    const File& file,
    uint32_t flags) {
  if (!base::Contains(regular_file_paths_, file.url)) {
    base::FilePath path;
    bool create_success =
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &path);
    DCHECK(create_success);
    bool write_success = base::WriteFile(path, file.content);
    DCHECK(write_success);
    regular_file_paths_[file.url] = path;
  }
  base::File regular_file(regular_file_paths_[file.url], flags);
  DCHECK(regular_file.IsValid());
  return base::ScopedFD(regular_file.TakePlatformFile());
}

base::ScopedFD FakeFileSystemInstance::CreateStreamFileDescriptorToRead(
    const std::string& content) {
  int fds[2];
  int ret = pipe(fds);
  DCHECK_EQ(0, ret);
  base::ScopedFD fd_read(fds[0]);
  base::ScopedFD fd_write(fds[1]);
  bool write_success = base::WriteFileDescriptor(fd_write.get(), content);
  DCHECK(write_success);
  return fd_read;
}

base::ScopedFD FakeFileSystemInstance::CreateStreamFileDescriptorToWrite(
    const std::string& url) {
  int fds[2];
  int ret = pipe(fds);
  DCHECK_EQ(0, ret);
  base::ScopedFD fd_read(fds[0]);
  base::ScopedFD fd_write(fds[1]);
  pipe_read_ends_.emplace(url, std::move(fd_read));
  return fd_write;
}

base::FilePath FakeFileSystemInstance::GetCrosPath(
    const base::FilePath& android_path) const {
  base::FilePath cros_path(cros_dir_);
  base::FilePath android_dir(kFakeAndroidPath);
  android_dir.AppendRelativePath(android_path, &cros_path);
  return cros_path;
}

}  // namespace arc
