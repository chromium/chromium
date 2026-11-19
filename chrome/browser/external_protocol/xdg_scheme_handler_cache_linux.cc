// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/xdg_scheme_handler_cache_linux.h"

#include <algorithm>
#include <initializer_list>
#include <map>
#include <memory>
#include <set>
#include <string_view>
#include <utility>

#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/nix/xdg_util.h"
#include "base/strings/cstring_view.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace {

constexpr std::string_view kSchemeHandlerPrefix = "x-scheme-handler/";

// mimeinfo.cache files are typically well under 1 MB.
constexpr size_t kMaxAssociationFileSize = 4 * 1024 * 1024;

// Coalesces bursts of file changes (e.g. an installer writing several files).
constexpr base::TimeDelta kRescanDelay = base::Milliseconds(500);

// Desktop file IDs map "-" to "/" for files in subdirectories (e.g.
// "kde4-foo.desktop" may be "kde4/foo.desktop"). Bound the number of
// replacements tried.
constexpr int kMaxDesktopIdSubdirLevels = 3;

XdgSchemeHandlerCache* g_instance = nullptr;

struct XdgDirs {
  std::vector<base::FilePath> config_dirs;
  std::vector<base::FilePath> applications_dirs;
  std::vector<std::string> desktops;
};

// Appends `item` to `items` unless it is already there. The lists are short,
// and the first occurrence has the highest precedence.
template <typename T>
void AppendUnique(std::vector<T>& items, T item) {
  if (!std::ranges::contains(items, item)) {
    items.push_back(std::move(item));
  }
}

// Returns $`env_var` if it is an absolute path, else ~/`fallback_dir`. The XDG
// Base Directory specification says relative paths must be ignored.
base::FilePath GetXdgHomeDir(base::Environment& env,
                             base::cstring_view env_var,
                             const char* fallback_dir) {
  base::FilePath dir = base::FilePath(env.GetVar(env_var).value_or(""))
                           .StripTrailingSeparators();
  if (dir.IsAbsolute()) {
    return dir;
  }
  return base::GetHomeDir().Append(fallback_dir);
}

// Returns the absolute paths in the colon-separated $`env_var`, or `defaults`
// if there are none.
std::vector<base::FilePath> GetXdgSearchDirs(
    base::Environment& env,
    base::cstring_view env_var,
    std::initializer_list<std::string_view> defaults) {
  std::vector<base::FilePath> dirs;
  for (std::string_view dir : base::SplitStringPiece(
           env.GetVar(env_var).value_or(""), ":", base::TRIM_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    base::FilePath path = base::FilePath(dir).StripTrailingSeparators();
    if (path.IsAbsolute()) {
      AppendUnique(dirs, std::move(path));
    }
  }
  if (dirs.empty()) {
    for (std::string_view dir : defaults) {
      dirs.emplace_back(dir);
    }
  }
  return dirs;
}

XdgDirs GetXdgDirs() {
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  XdgDirs dirs;

  AppendUnique(dirs.config_dirs,
               GetXdgHomeDir(*env, base::nix::kXdgConfigHomeEnvVar, ".config"));
  for (base::FilePath& dir :
       GetXdgSearchDirs(*env, "XDG_CONFIG_DIRS", {"/etc/xdg"})) {
    AppendUnique(dirs.config_dirs, std::move(dir));
  }

  AppendUnique(dirs.applications_dirs,
               GetXdgHomeDir(*env, "XDG_DATA_HOME", ".local/share")
                   .Append("applications"));
  for (const base::FilePath& dir : GetXdgSearchDirs(
           *env, "XDG_DATA_DIRS", {"/usr/local/share", "/usr/share"})) {
    AppendUnique(dirs.applications_dirs, dir.Append("applications"));
  }

  for (std::string_view desktop : base::SplitStringPiece(
           env->GetVar(base::nix::kXdgCurrentDesktopEnvVar).value_or(""), ":",
           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    AppendUnique(dirs.desktops, base::ToLowerASCII(desktop));
  }
  return dirs;
}

// Returns the association files in decreasing order of precedence, as listed
// by the XDG MIME Applications specification. Each applications dir's
// mimeinfo.cache (the associations from its desktop files' MimeType keys)
// comes right after that dir's mimeapps.list. Relative dirs and duplicate
// files are skipped.
std::vector<base::FilePath> GetAssociationFiles(
    const std::vector<base::FilePath>& config_dirs,
    const std::vector<base::FilePath>& applications_dirs,
    const std::vector<std::string>& desktops) {
  std::vector<base::FilePath> files;
  auto add_mimeapps_lists = [&](const base::FilePath& dir) {
    for (const std::string& desktop : desktops) {
      // Desktop names become part of a file name.
      if (!desktop.empty() && desktop.find('/') == std::string::npos &&
          desktop.find('\0') == std::string::npos) {
        AppendUnique(files, dir.Append(desktop + "-mimeapps.list"));
      }
    }
    AppendUnique(files, dir.Append("mimeapps.list"));
  };
  for (const base::FilePath& dir : config_dirs) {
    if (dir.IsAbsolute()) {
      add_mimeapps_lists(dir);
    }
  }
  for (const base::FilePath& dir : applications_dirs) {
    if (dir.IsAbsolute()) {
      add_mimeapps_lists(dir);
      AppendUnique(files, dir.Append("mimeinfo.cache"));
    }
  }
  return files;
}

using SchemeAssociations =
    std::vector<std::pair<std::string, std::vector<std::string>>>;

struct AssociationFile {
  // From [Default Applications], [Added Associations] and [MIME Cache].
  SchemeAssociations added;
  // From [Removed Associations].
  SchemeAssociations removed;
};

AssociationFile ReadAssociationFile(const base::FilePath& path) {
  AssociationFile result;
  std::string contents;
  if (!base::ReadFileToStringWithMaxSize(path, &contents,
                                         kMaxAssociationFileSize)) {
    return result;
  }

  SchemeAssociations* group = nullptr;
  for (std::string_view line : base::SplitStringPiece(
           contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (line.starts_with('#')) {
      continue;
    }
    if (line.starts_with('[')) {
      if (line == "[Default Applications]" || line == "[Added Associations]" ||
          line == "[MIME Cache]") {
        group = &result.added;
      } else if (line == "[Removed Associations]") {
        group = &result.removed;
      } else {
        group = nullptr;
      }
      continue;
    }
    if (!group) {
      continue;
    }
    size_t equals = line.find('=');
    if (equals == std::string_view::npos) {
      continue;
    }
    std::string key = base::ToLowerASCII(
        base::TrimWhitespaceASCII(line.substr(0, equals), base::TRIM_ALL));
    if (!key.starts_with(kSchemeHandlerPrefix) ||
        key.size() == kSchemeHandlerPrefix.size()) {
      continue;
    }
    group->emplace_back(
        key.substr(kSchemeHandlerPrefix.size()),
        base::SplitString(line.substr(equals + 1), ";", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY));
  }
  return result;
}

// Returns whether `relative_path` names a file inside a directory.
bool IsContainedRelativePath(std::string_view relative_path) {
  base::FilePath path(relative_path);
  return !relative_path.empty() && !path.IsAbsolute() &&
         !path.ReferencesParent();
}

bool DesktopFileExists(const std::string& desktop_id,
                       const std::vector<base::FilePath>& applications_dirs) {
  if (!desktop_id.ends_with(".desktop") ||
      desktop_id.find('/') != std::string::npos ||
      desktop_id.find('\0') != std::string::npos) {
    return false;
  }
  for (const base::FilePath& dir : applications_dirs) {
    if (!dir.IsAbsolute()) {
      continue;
    }
    std::string relative_path = desktop_id;
    for (int i = 0; i <= kMaxDesktopIdSubdirLevels; ++i) {
      // Checked after every '-' to '/' replacement, since e.g. "..-foo"
      // becomes "../foo".
      if (!IsContainedRelativePath(relative_path)) {
        break;
      }
      base::File::Info info;
      if (base::GetFileInfo(dir.Append(relative_path), &info) &&
          !info.is_directory) {
        return true;
      }
      size_t dash = relative_path.find('-');
      if (dash == std::string::npos) {
        break;
      }
      relative_path[dash] = '/';
    }
  }
  return false;
}

}  // namespace

// Lives on a background sequence: scans the association files and watches
// them for changes.
class XdgSchemeHandlerCache::Backend {
 public:
  using ScanCallback =
      base::RepeatingCallback<void(base::flat_set<std::string>)>;

  explicit Backend(ScanCallback callback)
      : dirs_(GetXdgDirs()), callback_(std::move(callback)) {
    for (const base::FilePath& file : GetAssociationFiles(
             dirs_.config_dirs, dirs_.applications_dirs, dirs_.desktops)) {
      auto watcher = std::make_unique<base::FilePathWatcher>();
      // Watching works for files that don't exist yet; failures only lose
      // live updates.
      if (watcher->Watch(file, base::FilePathWatcher::Type::kNonRecursive,
                         base::BindRepeating(&Backend::OnFileChanged,
                                             weak_factory_.GetWeakPtr()))) {
        watchers_.push_back(std::move(watcher));
      }
    }
    Scan();
  }

  Backend(const Backend&) = delete;
  Backend& operator=(const Backend&) = delete;
  ~Backend() = default;

 private:
  void OnFileChanged(const base::FilePath& path, bool error) {
    if (!rescan_timer_.IsRunning()) {
      rescan_timer_.Start(
          FROM_HERE, kRescanDelay,
          base::BindOnce(&Backend::Scan, weak_factory_.GetWeakPtr()));
    }
  }

  void Scan() {
    callback_.Run(XdgSchemeHandlerCache::ScanForSchemeHandlers(
        dirs_.config_dirs, dirs_.applications_dirs, dirs_.desktops));
  }

  const XdgDirs dirs_;
  const ScanCallback callback_;
  std::vector<std::unique_ptr<base::FilePathWatcher>> watchers_;
  base::OneShotTimer rescan_timer_;
  base::WeakPtrFactory<Backend> weak_factory_{this};
};

// static
scoped_refptr<XdgSchemeHandlerCache> XdgSchemeHandlerCache::GetOrCreate() {
  if (g_instance) {
    return base::WrapRefCounted(g_instance);
  }
  return base::WrapRefCounted(new XdgSchemeHandlerCache());
}

XdgSchemeHandlerCache::XdgSchemeHandlerCache() {
  CHECK(!g_instance);
  g_instance = this;
  backend_ = base::SequenceBound<Backend>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &XdgSchemeHandlerCache::OnScanned, weak_factory_.GetWeakPtr())));
}

XdgSchemeHandlerCache::~XdgSchemeHandlerCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

bool XdgSchemeHandlerCache::HasHandler(std::string_view scheme) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return schemes_.contains(base::ToLowerASCII(scheme));
}

void XdgSchemeHandlerCache::SetOnUpdatedCallbackForTesting(
    base::RepeatingClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_updated_for_testing_ = std::move(callback);
}

void XdgSchemeHandlerCache::OnScanned(base::flat_set<std::string> schemes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  schemes_ = std::move(schemes);
  if (on_updated_for_testing_) {
    on_updated_for_testing_.Run();
  }
}

// static
base::flat_set<std::string> XdgSchemeHandlerCache::ScanForSchemeHandlers(
    const std::vector<base::FilePath>& config_dirs,
    const std::vector<base::FilePath>& applications_dirs,
    const std::vector<std::string>& desktops) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::set<std::string> schemes;
  // Desktop IDs removed for a scheme by the files read so far. A removal hides
  // the association in the same file and in less important files.
  std::map<std::string, std::set<std::string>> removed;
  for (const base::FilePath& file :
       GetAssociationFiles(config_dirs, applications_dirs, desktops)) {
    AssociationFile associations = ReadAssociationFile(file);
    for (auto& [scheme, ids] : associations.removed) {
      removed[scheme].insert(ids.begin(), ids.end());
    }
    for (const auto& [scheme, ids] : associations.added) {
      if (schemes.contains(scheme)) {
        continue;
      }
      auto removed_it = removed.find(scheme);
      for (const std::string& id : ids) {
        if (removed_it != removed.end() && removed_it->second.contains(id)) {
          continue;
        }
        if (DesktopFileExists(id, applications_dirs)) {
          schemes.insert(scheme);
          break;
        }
      }
    }
  }
  return base::flat_set<std::string>(schemes.begin(), schemes.end());
}
