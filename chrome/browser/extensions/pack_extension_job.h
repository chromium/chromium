// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PACK_EXTENSION_JOB_H_
#define CHROME_BROWSER_EXTENSIONS_PACK_EXTENSION_JOB_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "extensions/browser/extension_creator.h"

namespace extensions {

// Manages packing an extension on the file thread and reporting the result
// back to the UI.
// Ownership note: In "asynchronous" mode, |Client| has to make sure this
// class's instances are kept alive until OnPackSuccess|OnPackFailure is called.
// Therefore this class assumes that posting task with base::Unretained(this)
// is safe.
class PackExtensionJob {
 public:
  // Interface for people who want to use PackExtensionJob to implement.
  class Client {
   public:
    virtual void OnPackSuccess(const base::FilePath& crx_file,
                               const base::FilePath& key_file) = 0;
    virtual void OnPackFailure(const std::string& message,
                               ExtensionCreator::ErrorType error_type) = 0;

   protected:
    virtual ~Client() {}
  };

  PackExtensionJob(Client* client,
                   const base::FilePath& root_directory,
                   const base::FilePath& key_file,
                   int run_flags);
  ~PackExtensionJob();

  // Starts the packing job.
  void Start();

  // The standard packing success message.
  static base::string16 StandardSuccessMessage(const base::FilePath& crx_file,
                                         const base::FilePath& key_file);

  void set_synchronous() { run_mode_ = RunMode::SYNCHRONOUS; }

 private:
  enum class RunMode { SYNCHRONOUS, ASYNCHRONOUS };

  // If |run_mode_| is SYNCHRONOUS, this is run on whichever thread calls it.
  void Run(scoped_refptr<base::SequencedTaskRunner> async_reply_task_runner);
  void ReportSuccessOnClientSequence(
      std::unique_ptr<base::FilePath> crx_file_out,
      std::unique_ptr<base::FilePath> key_file_out);
  void ReportFailureOnClientSequence(const std::string& error,
                                     ExtensionCreator::ErrorType error_type);

  Client* const client_;  // Owns us.
  base::FilePath root_directory_;
  base::FilePath key_file_;
  RunMode run_mode_ = RunMode::ASYNCHRONOUS;
  int run_flags_;  // Bitset of ExtensionCreator::RunFlags values - we always
                   // assume kRequireModernManifestVersion, though.

  // Used to check methods that run on |client_|'s sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PackExtensionJob);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PACK_EXTENSION_JOB_H_
