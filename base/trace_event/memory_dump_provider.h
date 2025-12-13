// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MEMORY_DUMP_PROVIDER_H_
#define BASE_TRACE_EVENT_MEMORY_DUMP_PROVIDER_H_

#include <compare>
#include <ostream>
#include <string>
#include <string_view>

#include "base/base_export.h"
#include "base/notreached.h"
#include "base/trace_event/memory_dump_provider_name_variants.h"
#include "base/trace_event/memory_dump_request_args.h"

namespace base {
namespace trace_event {

class ProcessMemoryDump;

// The contract interface that memory dump providers must implement.
class BASE_EXPORT MemoryDumpProvider {
 public:
  // Optional arguments for MemoryDumpManager::RegisterDumpProvider().
  struct Options {
    Options() : dumps_on_single_thread_task_runner(false) {}

    // |dumps_on_single_thread_task_runner| is true if the dump provider runs on
    // a SingleThreadTaskRunner, which is usually the case. It is faster to run
    // all providers that run on the same thread together without thread hops.
    bool dumps_on_single_thread_task_runner;
  };

  // A wrapper class to
  //
  // 1. Convert (often implicitly) a static const char* string into a name param
  // for MemoryDumpManager::RegisterDumpProvider().
  //
  // 2. Check that the name is in the MemoryDumpProviderName histogram variant
  // list, at compile time.
  //
  // The histogram variant name is formed by replacing ":" characters in the
  // static name with "_" (eg. "gpu::TextureManager" converts to
  // "gpu__TextureManager") because ":" has a special meaning in the histograms
  // dashboard.
  //
  // There is nothing special to do to use this class. For example, the
  // following works out of the box:
  //
  // memory_dump_manager->RegisterDumpProvider(provider, "Name", nullptr);
  //
  // However if the name is being passed through an intermediate function that
  // takes generic params, such as `make_unique`, the constructor must be
  // explicitly invoked. For example:
  //
  // auto proxy = std::make_unique<MemoryDumpProviderProxy>(
  //   MemoryDumpProvider::Name("Name"),
  //   ...);
  class Name {
   public:
    // Purposely not explicit to avoid requiring callers to wrap their string.
    // Since this is consteval the value of `name` is validated at compile time.
    consteval Name(const char* name) : static_name_(name) {
      if (!trace_event_metrics::IsValidMemoryDumpProviderName(
              histogram_name())) {
        // This will never actually invoke what's under NOTREACHED(), but
        // NOTREACHED() is invalid in a consteval context so compilation will
        // fail iff the string is invalid.
        NOTREACHED()
            << "Invalid provider name. Did you add it to the "
               "MemoryDumpProviderName variant in memory/histograms.xml?";
      }
    }

    // Return the name passed to the constructor.
    constexpr std::string_view static_name() const { return static_name_; }

    // Return a variant of the name to use in histograms.
    constexpr std::string histogram_name() const {
      std::string name(static_name_);
      size_t pos = 0;
      while ((pos = name.find(':', pos)) != std::string::npos) {
        name[pos] = '_';
      }
      return name;
    }

    // Comparators.
    constexpr friend bool operator==(const Name& a, const Name& b) = default;
    constexpr friend auto operator<=>(const Name& a, const Name& b) = default;

   private:
    std::string_view static_name_;
  };

  MemoryDumpProvider(const MemoryDumpProvider&) = delete;
  MemoryDumpProvider& operator=(const MemoryDumpProvider&) = delete;
  virtual ~MemoryDumpProvider() = default;

  // Called by the MemoryDumpManager when generating memory dumps.
  // The |args| specify if the embedder should generate light/heavy dumps on
  // dump requests. The embedder should return true if the |pmd| was
  // successfully populated, false if something went wrong and the dump should
  // be considered invalid.
  // (Note, the MemoryDumpManager has a fail-safe logic which will disable the
  // MemoryDumpProvider for the entire trace session if it fails consistently).
  virtual bool OnMemoryDump(const MemoryDumpArgs& args,
                            ProcessMemoryDump* pmd) = 0;

 protected:
  MemoryDumpProvider() = default;
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_MEMORY_DUMP_PROVIDER_H_
