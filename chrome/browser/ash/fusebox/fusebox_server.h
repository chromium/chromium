// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_SERVER_H_
#define CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_SERVER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "chrome/browser/ash/fusebox/fusebox_moniker.h"
#include "chromeos/ash/components/dbus/fusebox/fusebox.pb.h"

namespace fusebox {

class Server {
 public:
  // Returns a pointer to the global Server instance.
  static Server* GetInstance();

  Server();
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;
  ~Server();

  // Manages monikers in the context of the Server's MonikerMap.
  fusebox::Moniker CreateMoniker(storage::FileSystemURL target);
  void DestroyMoniker(fusebox::Moniker moniker);

  // These methods map 1:1 to the D-Bus methods implemented by
  // fusebox_service_provider.cc.
  //
  // In terms of semantics, they're roughly equivalent to the C standard
  // library functions of the same name. For example, the Stat method here
  // corresponds to the standard stat function described by "man 2 stat".
  //
  // These methods take a fs_url_as_string argument, roughly equivalent to a
  // POSIX filename that identifies a file or directory, but are a
  // storage::FileSystemURL (in string form).

  // Close is a placeholder and is not implemented yet.
  //
  // TODO(crbug.com/1249754) implement MTP device writing.
  using CloseCallback = base::OnceCallback<void(base::File::Error error_code)>;
  void Close(std::string fs_url_as_string, CloseCallback callback);

  // Open is a placeholder and is not implemented yet.
  //
  // TODO(crbug.com/1249754) implement MTP device writing.
  using OpenCallback = base::OnceCallback<void(base::File::Error error_code)>;
  void Open(std::string fs_url_as_string, OpenCallback callback);

  // Read returns the file's byte contents at the given offset and length.
  using ReadCallback = base::OnceCallback<void(base::File::Error error_code,
                                               const uint8_t* data_ptr,
                                               size_t data_len)>;
  void Read(std::string fs_url_as_string,
            int64_t offset,
            int32_t length,
            ReadCallback callback);

  // ReadDir lists the directory's children. The results may be sent back over
  // multiple RPC messages, each with the same client-chosen cookie value.
  using ReadDirCallback =
      base::RepeatingCallback<void(uint64_t cookie,
                                   base::File::Error error_code,
                                   fusebox::DirEntryListProto dir_entry_list,
                                   bool has_more)>;
  void ReadDir(std::string fs_url_as_string,
               uint64_t cookie,
               ReadDirCallback callback);

  // Stat returns the file or directory's metadata.
  using StatCallback = base::OnceCallback<void(base::File::Error error_code,
                                               const base::File::Info& info)>;
  void Stat(std::string fs_url_as_string, StatCallback callback);

 private:
  fusebox::MonikerMap moniker_map_;
};

}  // namespace fusebox

#endif  // CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_SERVER_H_
