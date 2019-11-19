// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_PROCESS_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_PROCESS_HOST_H_

#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_POSIX)
#include "base/files/file_descriptor_watcher_posix.h"
#endif

namespace net {

class DrainableIOBuffer;
class FileStream;
class IOBuffer;
class IOBufferWithSize;

}  // namespace net

namespace extensions {

// Manages the native side of a connection between an extension and a native
// process.
//
// This class must only be created, called, and deleted on the IO thread.
// Public methods typically accept callbacks which will be invoked on the UI
// thread.
class NativeMessageProcessHost : public NativeMessageHost {
 public:
  ~NativeMessageProcessHost() override;

  // Create using specified |launcher|. Used in tests.
  static std::unique_ptr<NativeMessageHost> CreateWithLauncher(
      const std::string& source_extension_id,
      const std::string& native_host_name,
      std::unique_ptr<NativeProcessLauncher> launcher);

  // extensions::NativeMessageHost implementation.
  void OnMessage(const std::string& message) override;
  void Start(Client* client) override;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override;

 private:
  NativeMessageProcessHost(const std::string& source_extension_id,
                           const std::string& native_host_name,
                           std::unique_ptr<NativeProcessLauncher> launcher);

  // Starts the host process.
  void LaunchHostProcess();

  // Callback for NativeProcessLauncher::Launch().
  void OnHostProcessLaunched(NativeProcessLauncher::LaunchResult result,
                             base::Process process,
                             base::File read_file,
                             base::File write_file);

  // Helper methods to read incoming messages.
  void WaitRead();
  void DoRead();
  void OnRead(int result);
  void HandleReadResult(int result);
  void ProcessIncomingData(const char* data, int data_size);

  // Helper methods to write outgoing messages.
  void DoWrite();
  void HandleWriteResult(int result);
  void OnWritten(int result);

  // Closes the connection and reports the |error_message| to the client.
  void Close(const std::string& error_message);

  // The Client messages will be posted to. Should only be accessed from the
  // UI thread.
  Client* client_;

  // ID of the calling extension.
  std::string source_extension_id_;

  // Name of the native messaging host.
  std::string native_host_name_;

  // Launcher used to launch the native process.
  std::unique_ptr<NativeProcessLauncher> launcher_;

  // Set to true after the native messaging connection has been stopped, e.g.
  // due to an error.
  bool closed_;

  base::Process process_;

  // Input stream reader.
  std::unique_ptr<net::FileStream> read_stream_;

#if defined(OS_POSIX)
  base::PlatformFile read_file_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> read_controller_;
#endif  // !defined(OS_POSIX)

  // Write stream.
  std::unique_ptr<net::FileStream> write_stream_;

  // Read buffer passed to FileStream::Read().
  scoped_refptr<net::IOBuffer> read_buffer_;

  // Set to true when a read is pending.
  bool read_pending_;

  // Buffer for incomplete incoming messages.
  std::string incoming_data_;

  // Queue for outgoing messages.
  base::queue<scoped_refptr<net::IOBufferWithSize>> write_queue_;

  // The message that's currently being sent.
  scoped_refptr<net::DrainableIOBuffer> current_write_buffer_;

  // Set to true when a write is pending.
  bool write_pending_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<NativeMessageProcessHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NativeMessageProcessHost);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_PROCESS_HOST_H_
