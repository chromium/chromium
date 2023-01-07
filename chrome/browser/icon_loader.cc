// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

// static
void IconLoader::LoadIcon(const base::FilePath& file_path,
                          IconSize size,
                          float scale,
                          IconLoadedCallback callback) {
  (new IconLoader(file_path, size, scale, std::move(callback)))->Start();
}

IconLoader::IconLoader(const base::FilePath& file_path,
                       IconSize size,
                       float scale,
                       IconLoadedCallback callback)
    : file_path_(file_path),
#if !BUILDFLAG(IS_ANDROID)
      icon_size_(size),
#endif
      scale_(scale),
      callback_(std::move(callback)) {
}

IconLoader::~IconLoader() = default;

#if !BUILDFLAG(IS_CHROMEOS)
void IconLoader::Start() {
  target_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();

  base::ThreadPool::PostTask(
      FROM_HERE, traits(),
      base::BindOnce(&IconLoader::ReadGroup, base::Unretained(this)));
}

#if !BUILDFLAG(IS_WIN)
void IconLoader::ReadGroup() {
  group_ = GroupForFilepath(file_path_);

  GetReadIconTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&IconLoader::ReadIcon, base::Unretained(this)));
}
#endif  // !BUILDFLAG(IS_WIN)
#endif  // !BUILDFLAG(IS_CHROMEOS)
