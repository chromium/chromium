// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_tracing.h"

#include <atomic>

#include "base/files/file.h"
#include "base/trace_event/base_tracing.h"

namespace base {

namespace {
std::atomic<FileTracing::Provider*> g_provider;
}

FileTracing::Provider* GetProvider() {
  return g_provider.load(std::memory_order_acquire);
}

// static
bool FileTracing::IsCategoryEnabled() {
  FileTracing::Provider* provider = GetProvider();
  return provider && provider->FileTracingCategoryIsEnabled();
}

// static
void FileTracing::SetProvider(FileTracing::Provider* provider) {
  g_provider.store(provider, std::memory_order_release);
}

FileTracing::ScopedEnabler::ScopedEnabler() {
  FileTracing::Provider* provider = GetProvider();
  if (provider)
    provider->FileTracingEnable(this);
}

FileTracing::ScopedEnabler::~ScopedEnabler() {
  FileTracing::Provider* provider = GetProvider();
  if (provider)
    provider->FileTracingDisable(this);
}

FileTracing::ScopedTrace::~ScopedTrace() {
  if (id_) {
    FileTracing::Provider* provider = GetProvider();
    if (provider)
      provider->FileTracingEventEnd(name_, id_);
  }
}

void FileTracing::ScopedTrace::Initialize(const char* name,
                                          const File* file,
                                          int64_t size) {
  id_ = &file->trace_enabler_;
  name_ = name;
  GetProvider()->FileTracingEventBegin(name_, id_, file->path_, size);
}

}  // namespace base
