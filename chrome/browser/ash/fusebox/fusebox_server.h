// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_SERVER_H_
#define CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_SERVER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/fusebox/fusebox.pb.h"
#include "chrome/browser/ash/fusebox/fusebox_moniker.h"
#include "chrome/browser/ash/fusebox/fusebox_staging.pb.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/file_system_context.h"

class Profile;

namespace fusebox {

class Server {
 public:
  struct Delegate {
    // These methods cause D-Bus signals to be sent that a storage unit (as
    // named by the "subdir" in "/media/fuse/fusebox/subdir") has been attached
    // or detached.
    virtual void OnRegisterFSURLPrefix(const std::string& subdir) = 0;
    virtual void OnUnregisterFSURLPrefix(const std::string& subdir) = 0;
  };

  // Returns a pointer to the global Server instance.
  static Server* GetInstance();

  // Returns POSIX style (S_IFREG | rwxr-x---) bits.
  static uint32_t MakeModeBits(bool is_directory, bool read_only);

  // The delegate should live longer than the server.
  explicit Server(Delegate* delegate);
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;
  ~Server();

  // Manages monikers in the context of the Server's MonikerMap.
  fusebox::Moniker CreateMoniker(const storage::FileSystemURL& target,
                                 bool read_only);
  void DestroyMoniker(fusebox::Moniker moniker);

  void RegisterFSURLPrefix(const std::string& subdir,
                           const std::string& fs_url_prefix,
                           bool read_only);
  void UnregisterFSURLPrefix(const std::string& subdir);

  // Converts a FuseBox filename (e.g. "/media/fuse/fusebox/subdir/p/q.txt") to
  // a storage::FileSystemURL, substituting the fs_url_prefix for "/etc/subdir"
  // according to previous RegisterFSURLPrefix calls. The "/p/q.txt" suffix may
  // be empty but "subdir" (and everything prior) must be present.
  //
  // If "subdir" mapped to "filesystem:origin/external/mount_name/xxx/yyy" then
  // this returns "filesystem:origin/external/mount_name/xxx/yyy/p/q.txt" in
  // storage::FileSystemURL form.
  //
  // It returns an invalid storage::FileSystemURL if the filename doesn't match
  // "/media/fuse/fusebox/subdir/etc" or the "subdir" wasn't registered.
  storage::FileSystemURL ResolveFilename(Profile* profile,
                                         const std::string& filename);

  // Returns human-readable debugging information as a JSON value.
  base::Value GetDebugJSON();

  // These methods map 1:1 to the D-Bus methods implemented by
  // fusebox_service_provider.cc.
  //
  // For the "file operation D-Bus methods" (below until "Meta D-Bus methods")
  // in terms of semantics, they're roughly equivalent to the C standard
  // library functions of the same name. For example, the Stat method here
  // corresponds to the standard stat function described by "man 2 stat".
  //
  // These methods take a fs_url_as_string argument, roughly equivalent to a
  // POSIX filename that identifies a file or directory, but are a
  // storage::FileSystemURL (in string form).

  // Close is a placeholder and is not implemented yet.
  //
  // TODO(crbug.com/1249754) implement MTP device writing.
  using CloseCallback = base::OnceCallback<void(int32_t posix_error_code)>;
  void Close(const std::string& fs_url_as_string, CloseCallback callback);

  // MkDir is analogous to "/usr/bin/mkdir".
  using MkDirCallback = base::OnceCallback<void(
      const fusebox_staging::MkDirResponseProto& response)>;
  void MkDir(const fusebox_staging::MkDirRequestProto& request,
             MkDirCallback callback);

  // Open is a placeholder and is not implemented yet.
  //
  // TODO(crbug.com/1249754) implement MTP device writing.
  using OpenCallback = base::OnceCallback<void(int32_t posix_error_code)>;
  void Open(const std::string& fs_url_as_string, OpenCallback callback);

  // Read returns the file's byte contents at the given offset and length.
  using ReadCallback = base::OnceCallback<
      void(int32_t posix_error_code, const uint8_t* data_ptr, size_t data_len)>;
  void Read(const std::string& fs_url_as_string,
            int64_t offset,
            int32_t length,
            ReadCallback callback);

  // ReadDir2 lists the directory's children. The results will be sent back in
  // the responses of one or more request-response RPC pairs. The first request
  // and last response have a zero cookie value. The remaining RPCs will have
  // the same server-chosen, non-zero cookie value.
  //
  // The request's cancel_error_code is typically zero but if not, it is echoed
  // in the response (which becomes the final response) and indicates that the
  // D-Bus client is cancelling the overall "read a directory" operation.
  //
  // TODO(crbug.com/1363861): document the D-Bus protocol separately.
  using ReadDir2Callback =
      base::OnceCallback<void(const ReadDir2ResponseProto& response)>;
  void ReadDir2(const ReadDir2RequestProto& request, ReadDir2Callback callback);

  // RmDir is analogous to "/usr/bin/rmdir".
  using RmDirCallback = base::OnceCallback<void(
      const fusebox_staging::RmDirResponseProto& response)>;
  void RmDir(const fusebox_staging::RmDirRequestProto& request,
             RmDirCallback callback);

  // Stat returns the file or directory's metadata.
  using StatCallback = base::OnceCallback<void(int32_t posix_error_code,
                                               const base::File::Info& info,
                                               bool read_only)>;
  void Stat(const std::string& fs_url_as_string, StatCallback callback);

  // File operation D-Bus methods above. Meta D-Bus methods below, which do not
  // map 1:1 to FUSE or C standard library file operations.

  // ListStorages returns the active subdir names. Active means passed to
  // RegisterFSURLPrefix without a subsequent UnregisterFSURLPrefix.
  using ListStoragesCallback =
      base::OnceCallback<void(const ListStoragesResponseProto& response)>;
  void ListStorages(const ListStoragesRequestProto& request,
                    ListStoragesCallback callback);

  // MakeTempDir makes a temporary directory that has two file paths: an
  // underlying one (e.g. "/tmp/.foo") and a fusebox one (e.g.
  // "/media/fuse/fusebox/tmp.foo"). The fusebox one is conceptually similar to
  // a symbolic link, in that after a "touch /tmp/.foo/bar", bar should be
  // visible at "/media/fuse/fusebox/tmp.foo/bar", but the 'symbolic link' is
  // not resolved directly by the kernel.
  //
  // Instead, file I/O under the /media/fuse/fusebox mount point goes through
  // the FuseBox daemon (via FUSE) to Chromium (via D-Bus) to the kernel (as
  // Chromium storage::FileSystemURL code sees storage::kFileSystemTypeLocal
  // files living under the underlying file path).
  //
  // That sounds convoluted (and overkill for 'sym-linking' a directory on the
  // local file system), and it is, but it is essentially the same code paths
  // that FuseBox uses to surface Chromium virtual file systems (VFSs) that are
  // not otherwise visible on the kernel-level file system. Note that Chromium
  // VFSs are not the same as Linux kernel VFSs.
  //
  // The purpose of these Make/Remove methods is to facilitate testing these
  // FuseBox code paths (backed by an underlying tmpfs file system) without the
  // extra complexity of fake VFSs, such as a fake ADP (Android Documents
  // Provider) or fake MTP (Media Transfer Protocol) back-end.
  //
  // MakeTempDir is like "mkdir" (except the callee randomly generates the file
  // path). RemoveTempDir is like "rm -rf".
  using MakeTempDirCallback =
      base::OnceCallback<void(const std::string& error_message,
                              const std::string& fusebox_file_path,
                              const std::string& underlying_file_path)>;
  void MakeTempDir(MakeTempDirCallback callback);
  void RemoveTempDir(const std::string& fusebox_file_path);

  struct PrefixMapEntry {
    PrefixMapEntry(std::string fs_url_prefix_arg, bool read_only_arg);

    std::string fs_url_prefix;
    bool read_only;
  };

  // Maps from a subdir to a storage::FileSystemURL prefix in string form (and
  // other metadata). For example, the subdir could be the "foo" in the
  // "/media/fuse/fusebox/foo/bar/baz.txt" filename, which gets mapped to
  // "fs_url_prefix/bar/baz.txt" before that whole string is parsed as a
  // storage::FileSystemURL.
  //
  // Neither subdir nor fs_url_prefix should have a trailing slash.
  using PrefixMap = std::map<std::string, PrefixMapEntry>;

  struct ReadDir2MapEntry {
    explicit ReadDir2MapEntry(ReadDir2Callback callback);
    ReadDir2MapEntry(ReadDir2MapEntry&&);
    ~ReadDir2MapEntry();

    // Returns whether the final response was sent.
    bool Reply(uint64_t cookie, ReadDir2Callback callback);

    int32_t posix_error_code_ = 0;
    ReadDir2ResponseProto response_;
    bool has_more_ = true;

    ReadDir2Callback callback_;
  };

  // Maps from ReadDir2 cookies to a pair of (1) a buffer of upstream results
  // from Chromium's storage layer and (2) a possibly-hasnt-arrived-yet pending
  // downstream ReadDir2Callback (i.e. a D-Bus RPC response).
  //
  // If the upstream layer sends its results first then we need to buffer until
  // we have a downstream callback to pass those results onto.
  //
  // If the downstream layer sends its callback first then we need to hold onto
  // it until we have results to pass on.
  //
  // Note that the upstream API works with a base::RepeatingCallback model (one
  // request, multiple responses) but the downstream API (i.e. D-Bus) works
  // with a base::OnceCallback model (N requests, N responses).
  using ReadDir2Map = std::map<uint64_t, ReadDir2MapEntry>;

  // Maps from a fusebox_file_path (like "/media/fuse/fusebox/tmp.foo") to the
  // ScopedTempDir that will clean up (in its destructor) the underlying
  // temporary directory.
  using TempSubdirMap = std::map<std::string, base::ScopedTempDir>;

 private:
  void MakeTempDirOnWorkerThread(MakeTempDirCallback callback);
  void ReplyToMakeTempDir(base::ScopedTempDir scoped_temp_dir,
                          bool create_succeeded,
                          MakeTempDirCallback callback);

  void OnReadDirectory(scoped_refptr<storage::FileSystemContext> fs_context,
                       bool read_only,
                       uint64_t cookie,
                       base::File::Error error_code,
                       storage::AsyncFileUtil::EntryList entry_list,
                       bool has_more);

  Delegate* delegate_;
  fusebox::MonikerMap moniker_map_;
  PrefixMap prefix_map_;
  ReadDir2Map read_dir_2_map_;
  TempSubdirMap temp_subdir_map_;

  base::WeakPtrFactory<Server> weak_ptr_factory_{this};
};

}  // namespace fusebox

#endif  // CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_SERVER_H_
