// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_LOGGING_OBSERVER_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_LOGGING_OBSERVER_H_

#include "chrome/browser/ash/file_system_provider/observer.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"

namespace ash::file_system_provider {

// Utility observer, logging events from file_system_provider::Service.
class LoggingObserver : public Observer {
 public:
  class Event {
   public:
    Event(const ProvidedFileSystemInfo& file_system_info,
          MountContext context,
          base::File::Error error)
        : file_system_info_(file_system_info),
          context_(context),
          error_(error) {}
    ~Event() = default;

    const ProvidedFileSystemInfo& file_system_info() const {
      return file_system_info_;
    }
    MountContext context() const { return context_; }
    base::File::Error error() const { return error_; }

   private:
    ProvidedFileSystemInfo file_system_info_;
    MountContext context_;
    base::File::Error error_;
  };

  LoggingObserver();

  LoggingObserver(const LoggingObserver&) = delete;
  LoggingObserver& operator=(const LoggingObserver&) = delete;

  ~LoggingObserver() override;

  // file_system_provider::Observer overrides.
  void OnProvidedFileSystemMount(const ProvidedFileSystemInfo& file_system_info,
                                 MountContext context,
                                 base::File::Error error) override;

  void OnProvidedFileSystemUnmount(
      const ProvidedFileSystemInfo& file_system_info,
      base::File::Error error) override;

  std::vector<Event> mounts;
  std::vector<Event> unmounts;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_LOGGING_OBSERVER_H_
