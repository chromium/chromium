// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/nix/mime_util_xdg.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/stack.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/no_destructor.h"
#include "base/numerics/byte_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "base/synchronization/lock.h"
#endif

namespace base::nix {
namespace {

// Ridiculously large size for a /usr/share/mime/mime.cache file.
// Default file is about 100KB, allow up to 10MB.
constexpr size_t kMaxMimeTypesFileSize = 10 * 1024 * 1024;
// Maximum number of nodes to allow in reverse suffix tree.
// Default file has ~3K nodes, allow up to 30K.
constexpr size_t kMaxNodes = 30000;
// Maximum file extension size.
constexpr size_t kMaxExtSize = 100;
// Header size in mime.cache file.
constexpr size_t kHeaderSize = 40;
// Largest valid unicode code point is U+10ffff.
constexpr uint32_t kMaxUnicode = 0x10ffff;
// Default mime glob weight is 50, max is 100.
constexpr uint8_t kDefaultGlobWeight = 50;

// Path and last modified of mime.cache file.
struct FileInfo {
  FilePath path;
  Time last_modified;
};

// Load all mime cache files on the system.
void LoadAllMimeCacheFiles(MimeTypeMap& map, std::vector<FileInfo>& files) {
  std::unique_ptr<Environment> env(Environment::Create());
  File::Info info;
  for (const auto& path : GetXDGDataSearchLocations(env.get())) {
    FilePath mime_cache = path.Append("mime/mime.cache");
    if (GetFileInfo(mime_cache, &info) && ParseMimeTypes(mime_cache, map)) {
      files.emplace_back(mime_cache, info.last_modified);
    }
  }
}

// Read 4 bytes from string `buf` at `offset` as network order uint32_t.
// Returns false if `offset > buf.size() - 4` or `offset` is not aligned to a
// 4-byte word boundary, or `*result` is not between `min_result` and
// `max_result`. `field_name` is used in error message.
bool ReadInt(const std::string& buf,
             uint32_t offset,
             const std::string& field_name,
             uint32_t min_result,
             size_t max_result,
             uint32_t* result) {
  if (offset > buf.size() - 4 || (offset & 0x3)) {
    LOG(ERROR) << "Invalid offset=" << offset << " for " << field_name
               << ", string size=" << buf.size();
    return false;
  }
  auto bytes = base::as_byte_span(buf);
  *result = base::U32FromBigEndian(bytes.subspan(offset).first<4u>());
  if (*result < min_result || *result > max_result) {
    LOG(ERROR) << "Invalid " << field_name << "=" << *result
               << " not between min_result=" << min_result
               << " and max_result=" << max_result;
    return false;
  }
  return true;
}

}  // namespace

bool ParseMimeTypes(const FilePath& file_path, MimeTypeMap& out_mime_types) {
  // File format from
  // https://specifications.freedesktop.org/shared-mime-info-spec/shared-mime-info-spec-0.21.html#idm46070612075440
  // Header:
  // 2      CARD16    MAJOR_VERSION  1
  // 2      CARD16    MINOR_VERSION  2
  // 4      CARD32    ALIAS_LIST_OFFSET
  // 4      CARD32    PARENT_LIST_OFFSET
  // 4      CARD32    LITERAL_LIST_OFFSET
  // 4      CARD32    REVERSE_SUFFIX_TREE_OFFSET
  // ...
  // ReverseSuffixTree:
  // 4      CARD32    N_ROOTS
  // 4       CARD32    FIRST_ROOT_OFFSET
  // ReverseSuffixTreeNode:
  // 4      CARD32    CHARACTER
  // 4      CARD32    N_CHILDREN
  // 4      CARD32    FIRST_CHILD_OFFSET
  // ReverseSuffixTreeLeafNode:
  // 4      CARD32    0
  // 4      CARD32    MIME_TYPE_OFFSET
  // 4      CARD32    WEIGHT in lower 8 bits
  //                  FLAGS in rest:
  //                  0x100 = case-sensitive

  std::string buf;
  if (!ReadFileToStringWithMaxSize(file_path, &buf, kMaxMimeTypesFileSize)) {
    LOG(ERROR) << "Failed reading in mime.cache file: " << file_path;
    return false;
  }

  if (buf.size() < kHeaderSize) {
    LOG(ERROR) << "Invalid mime.cache file size=" << buf.size();
    return false;
  }

  // Validate file[ALIAS_LIST_OFFSET - 1] is null to ensure that any
  // null-terminated strings dereferenced at addresses below ALIAS_LIST_OFFSET
  // will not overflow.
  uint32_t alias_list_offset = 0;
  if (!ReadInt(buf, 4, "ALIAS_LIST_OFFSET", kHeaderSize, buf.size(),
               &alias_list_offset)) {
    return false;
  }
  if (buf[alias_list_offset - 1] != 0) {
    LOG(ERROR) << "Invalid mime.cache file does not contain null prior to "
                  "ALIAS_LIST_OFFSET="
               << alias_list_offset;
    return false;
  }

  // Parse ReverseSuffixTree. Read all nodes and place them on `stack`,
  // allowing max of kMaxNodes and max extension of kMaxExtSize.
  uint32_t tree_offset = 0;
  if (!ReadInt(buf, 16, "REVERSE_SUFFIX_TREE_OFFSET", kHeaderSize, buf.size(),
               &tree_offset)) {
    return false;
  }

  struct Node {
    std::string ext;
    uint32_t n_children;
    uint32_t first_child_offset;
  };

  // Read root node and put it on the stack.
  Node root;
  if (!ReadInt(buf, tree_offset, "N_ROOTS", 0, kMaxUnicode, &root.n_children)) {
    return false;
  }
  if (!ReadInt(buf, tree_offset + 4, "FIRST_ROOT_OFFSET", tree_offset,
               buf.size(), &root.first_child_offset)) {
    return false;
  }
  stack<Node> stack;
  stack.push(std::move(root));

  uint32_t num_nodes = 0;
  while (!stack.empty()) {
    // Pop top node from the stack and process children.
    Node n = std::move(stack.top());
    stack.pop();
    uint32_t p = n.first_child_offset;
    for (uint32_t i = 0; i < n.n_children; i++) {
      uint32_t c = 0;
      if (!ReadInt(buf, p, "CHARACTER", 0, kMaxUnicode, &c)) {
        return false;
      }
      p += 4;

      // Leaf node, add mime type if it is highest weight.
      if (c == 0) {
        uint32_t mime_type_offset = 0;
        if (!ReadInt(buf, p, "mime type offset", kHeaderSize,
                     alias_list_offset - 1, &mime_type_offset)) {
          return false;
        }
        p += 4;
        uint8_t weight = kDefaultGlobWeight;
        if ((p + 3) < buf.size()) {
          weight = static_cast<uint8_t>(buf[p + 3]);
        }
        p += 4;
        if (n.ext.size() > 0 && n.ext[0] == '.') {
          std::string_view ext = std::string_view(n.ext).substr(1u);
          auto it = out_mime_types.find(ext);
          if (it == out_mime_types.end() || weight > it->second.weight) {
            // Use the mime type string from `buf` up to the first NUL.
            auto mime_type = std::string_view(buf).substr(mime_type_offset);
            mime_type = mime_type.substr(0u, mime_type.find('\0'));
            out_mime_types[std::string(ext)] = {std::string(mime_type), weight};
          }
        }
        continue;
      }

      // Regular node, parse and add it to the stack.
      Node node;
      WriteUnicodeCharacter(static_cast<int>(c), &node.ext);
      node.ext += n.ext;
      if (!ReadInt(buf, p, "N_CHILDREN", 0, kMaxUnicode, &node.n_children)) {
        return false;
      }
      p += 4;
      if (!ReadInt(buf, p, "FIRST_CHILD_OFFSET", tree_offset, buf.size(),
                   &node.first_child_offset)) {
        return false;
      }
      p += 4;

      // Check limits.
      if (++num_nodes > kMaxNodes) {
        LOG(ERROR) << "Exceeded maxium number of nodes=" << kMaxNodes;
        return false;
      }
      if (node.ext.size() > kMaxExtSize) {
        LOG(WARNING) << "Ignoring large extension exceeds size=" << kMaxExtSize
                     << " ext=" << node.ext;
        continue;
      }

      stack.push(std::move(node));
    }
  }

  return true;
}

std::string GetFileMimeType(const FilePath& filepath) {
  std::string ext = filepath.Extension();
  if (ext.empty()) {
    return std::string();
  }

  static NoDestructor<std::vector<FileInfo>> xdg_mime_files;

  static NoDestructor<MimeTypeMap> mime_type_map([] {
    MimeTypeMap map;
    LoadAllMimeCacheFiles(map, *xdg_mime_files);
    return map;
  }());

  // Files never change on ChromeOS, but for linux, match xdgmime behavior and
  // check every 5s and reload if any files have changed.
#if !BUILDFLAG(IS_CHROMEOS)
  static Time last_check;
  // Lock is required since this may be called on any thread.
  static NoDestructor<Lock> lock;
  {
    AutoLock scoped_lock(*lock);

    Time now = Time::Now();
    if (last_check + Seconds(5) < now) {
      if (ranges::any_of(*xdg_mime_files, [](const FileInfo& file_info) {
            File::Info info;
            return !GetFileInfo(file_info.path, &info) ||
                   info.last_modified != file_info.last_modified;
          })) {
        mime_type_map->clear();
        xdg_mime_files->clear();
        LoadAllMimeCacheFiles(*mime_type_map, *xdg_mime_files);
      }
      last_check = now;
    }
  }
#endif

  auto it = mime_type_map->find(ext.substr(1));
  return it != mime_type_map->end() ? it->second.mime_type : std::string();
}

}  // namespace base::nix
