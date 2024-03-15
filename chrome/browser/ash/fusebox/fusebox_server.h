// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_SERVER_H_
#define CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_SERVER_H_

#include <string>

#include "base/containers/circular_deque.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/values.h"
#include "chrome/browser/ash/fusebox/fusebox.pb.h"
#include "chrome/browser/ash/fusebox/fusebox_histograms.h"
#include "chrome/browser/ash/fusebox/fusebox_moniker.h"
#include "chrome/browser/ash/system_web_apps/apps/files_internals_debug_json_provider.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class Profile;

namespace fusebox {

class ReadWriter;

class Server : public ash::FilesInternalsDebugJSONProvider {
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
  ~Server() override;

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

  // Performs the inverse of ResolveFilename. It converts a FileSystemURL like
  // "filesystem:origin/external/mount_name/xxx/yyy/p/q.txt" to a FuseBox
  // filename like "/media/fuse/fusebox/subdir/p/q.txt".
  //
  // It returns an empty base::FilePath on failure, such as when there was no
  // previously registered (subdir, fs_url_prefix) that matched.
  base::FilePath InverseResolveFSURL(const storage::FileSystemURL& fs_url);

  // Chains GetInstance and InverseResolveFSURL, returning an empty
  // base::FilePath when there is no instance.
  static base::FilePath SubstituteFuseboxFilePath(
      const storage::FileSystemURL& fs_url) {
    Server* server = GetInstance();
    return server ? server->InverseResolveFSURL(fs_url) : base::FilePath();
  }

  // ash::FilesInternalsDebugJSONProvider overrides.
  void GetDebugJSONForKey(
      std::string_view key,
      base::OnceCallback<void(JSONKeyValuePair)> callback) override;

  // These methods map 1:1 to the D-Bus methods implemented by
  // fusebox_service_provider.cc.
  //
  // For the "file operation D-Bus methods" (below until "Meta D-Bus methods")
  // in terms of semantics, they're roughly equivalent to the C standard
  // library functions of the same name. For example, the Stat method here
  // corresponds to the standard stat function described by "man 2 stat".
  //
  // These methods all take a protobuf argument and return (via a callback)
  // another protobuf. Many of the request protos have a string-typed
  // file_system_url field, roughly equivalent to a POSIX filename for a file
  // or directory. These used to be full storage::FileSystemURL strings (e.g.
  // "filesystem:chrome://file-manager/external/foo/com.bar/baz/p/q.txt") but
  // today look like "subdir/p/q.txt". The PrefixMap is used to resolve the
  // "subdir" prefix to recreate the storage::FileSystemURL.
  //
  // See system_api/dbus/fusebox/fusebox.proto for more commentary.

  // Close2 closes a virtual file opened by Open2.
  using Close2Callback =
      base::OnceCallback<void(const Close2ResponseProto& response)>;
  void Close2(const Close2RequestProto& request, Close2Callback callback);

  // Create creates a file (not a directory).
  using CreateCallback =
      base::OnceCallback<void(const CreateResponseProto& response)>;
  void Create(const CreateRequestProto& request, CreateCallback callback);

  // Flush flushes a file, like the C standard library's fsync.
  using FlushCallback =
      base::OnceCallback<void(const FlushResponseProto& response)>;
  void Flush(const FlushRequestProto& request, FlushCallback callback);

  // MkDir is analogous to "/usr/bin/mkdir".
  using MkDirCallback =
      base::OnceCallback<void(const MkDirResponseProto& response)>;
  void MkDir(const MkDirRequestProto& request, MkDirCallback callback);

  // Open2 opens a virtual file for reading and/or writing.
  using Open2Callback =
      base::OnceCallback<void(const Open2ResponseProto& response)>;
  void Open2(const Open2RequestProto& request, Open2Callback callback);

  // Rename is analogous to "/usr/bin/mv".
  using RenameCallback =
      base::OnceCallback<void(const RenameResponseProto& response)>;
  void Rename(const RenameRequestProto& request, RenameCallback callback);

  // Read2 reads from a virtual file opened by Open2.
  using Read2Callback =
      base::OnceCallback<void(const Read2ResponseProto& response)>;
  void Read2(const Read2RequestProto& request, Read2Callback callback);

  // ReadDir2 lists the directory's children.
  using ReadDir2Callback =
      base::OnceCallback<void(const ReadDir2ResponseProto& response)>;
  void ReadDir2(const ReadDir2RequestProto& request, ReadDir2Callback callback);

  // RmDir is analogous to "/usr/bin/rmdir".
  using RmDirCallback =
      base::OnceCallback<void(const RmDirResponseProto& response)>;
  void RmDir(const RmDirRequestProto& request, RmDirCallback callback);

  // Stat2 returns the file or directory's metadata.
  using Stat2Callback =
      base::OnceCallback<void(const Stat2ResponseProto& response)>;
  void Stat2(const Stat2RequestProto& request, Stat2Callback callback);

  // Truncate sets a file's size.
  using TruncateCallback =
      base::OnceCallback<void(const TruncateResponseProto& response)>;
  void Truncate(const TruncateRequestProto& request, TruncateCallback callback);

  // Unlink deletes a file.
  using UnlinkCallback =
      base::OnceCallback<void(const UnlinkResponseProto& response)>;
  void Unlink(const UnlinkRequestProto& request, UnlinkCallback callback);

  // Write2 writes to a virtual file opened by Open2.
  using Write2Callback =
      base::OnceCallback<void(const Write2ResponseProto& response)>;
  void Write2(const Write2RequestProto& request, Write2Callback callback);

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

  // ----

  using PendingFlush = std::pair<FlushRequestProto, FlushCallback>;
  using PendingRead2 = std::pair<Read2RequestProto, Read2Callback>;
  using PendingWrite2 = std::pair<Write2RequestProto, Write2Callback>;
  using PendingOp = absl::variant<PendingFlush, PendingRead2, PendingWrite2>;

  struct FuseFileMapEntry {
    FuseFileMapEntry(scoped_refptr<storage::FileSystemContext> fs_context_arg,
                     storage::FileSystemURL fs_url_arg,
                     const std::string& profile_path_arg,
                     bool readable_arg,
                     bool writable_arg,
                     bool use_temp_file_arg,
                     bool temp_file_starts_with_copy_arg);
    FuseFileMapEntry(FuseFileMapEntry&&);
    ~FuseFileMapEntry();

    void DoFlush(const FlushRequestProto& request, FlushCallback callback);
    void DoRead2(const Read2RequestProto& request, Read2Callback callback);
    void DoWrite2(const Write2RequestProto& request, Write2Callback callback);
    void Do(PendingOp& op,
            base::WeakPtr<Server> weak_ptr_server,
            uint64_t fuse_handle);

    const scoped_refptr<storage::FileSystemContext> fs_context_;
    const HistogramEnumFileSystemType histogram_enum_file_system_type_;
    const bool readable_;
    const bool writable_;

    bool has_in_flight_op_ = false;
    base::circular_deque<PendingOp> pending_ops_;

    base::SequenceBound<ReadWriter> seqbnd_read_writer_;
  };

  // Maps from fuse_handle uint64_t values to FileStreamReader /
  // FileStreamWriter state.
  using FuseFileMap = std::map<uint64_t, FuseFileMapEntry>;

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

  // ----

 private:
  void ReplyToMakeTempDir(base::ScopedTempDir scoped_temp_dir,
                          bool create_succeeded,
                          MakeTempDirCallback callback);

  void OnFlush(uint64_t fuse_handle,
               FlushCallback callback,
               const FlushResponseProto& response);

  void OnRead2(uint64_t fuse_handle,
               Read2Callback callback,
               const Read2ResponseProto& response);

  void OnReadDirectory(scoped_refptr<storage::FileSystemContext> fs_context,
                       bool read_only,
                       uint64_t cookie,
                       base::File::Error error_code,
                       storage::AsyncFileUtil::EntryList entry_list,
                       bool has_more);

  void OnWrite2(uint64_t fuse_handle,
                Write2Callback callback,
                const Write2ResponseProto& response);

  // Removes the entry (if present) for the given map key.
  void EraseFuseFileMapEntry(uint64_t fuse_handle);
  // Returns the fuse_handle that is the map key.
  uint64_t InsertFuseFileMapEntry(FuseFileMapEntry&& entry);

  raw_ptr<Delegate> delegate_;
  FuseFileMap fuse_file_map_;
  fusebox::MonikerMap moniker_map_;
  PrefixMap prefix_map_;
  ReadDir2Map read_dir_2_map_;
  TempSubdirMap temp_subdir_map_;

  base::WeakPtrFactory<Server> weak_ptr_factory_{this};
};

}  // namespace fusebox

#endif  // CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_SERVER_H_
