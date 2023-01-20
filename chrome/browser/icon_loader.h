// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ICON_LOADER_H_
#define CHROME_BROWSER_ICON_LOADER_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/image/image.h"

////////////////////////////////////////////////////////////////////////////////
//
// A facility to read a file containing an icon asynchronously in the IO
// thread. Returns the icon in the form of an ImageSkia.
//
////////////////////////////////////////////////////////////////////////////////
class IconLoader {
 public:
  // An IconGroup is a class of files that all share the same icon.
#if BUILDFLAG(IS_MAC)
  // On the Mac, it's the UTType's identifier. (Apps do have unique icons, just
  // like in Windows, below, but `IconLoader` is never used to get their icons,
  // so that case isn't handled.)
  using IconGroup = std::string;
#else
  // On all other platforms except Windows, and for most files on Windows, it is
  // the file type (e.g. all .mp3 files share an icon, all .html files share an
  // icon). On Windows, for certain file types (.exe, .dll, etc), each file of
  // that type is assumed to have a unique icon. In that case, each of those
  // files is a group to itself.
  using IconGroup = base::FilePath::StringType;
#endif

  enum IconSize {
    SMALL = 0,  // 16x16
    NORMAL,     // 32x32
    LARGE,      // Windows: 32x32, Linux: 48x48, Mac: Unsupported
    ALL,        // All sizes available
  };

  // The callback invoked when an icon has been read. The parameters are:
  // - The icon that was loaded (IsEmpty() will be true on failure to load).
  // - The determined group from the original requested path.
  using IconLoadedCallback =
      base::OnceCallback<void(gfx::Image, const IconGroup&)>;

  // Starts the process of reading the icon. When the reading of the icon is
  // complete, the IconLoadedCallback callback will be fulfilled, and the
  // IconLoader will delete itself.
  static void LoadIcon(const base::FilePath& file_path,
                       IconSize size,
                       float scale,
                       IconLoadedCallback callback);

  IconLoader(const IconLoader&) = delete;
  IconLoader& operator=(const IconLoader&) = delete;

 private:
  IconLoader(const base::FilePath& file_path,
             IconSize size,
             float scale,
             IconLoadedCallback callback);

  ~IconLoader();

  void Start();

  // Given a file path, get the group for the given file.
  static IconGroup GroupForFilepath(const base::FilePath& file_path);

  // The TaskRunner that ReadIcon() must be called on.
  static scoped_refptr<base::TaskRunner> GetReadIconTaskRunner();

#if !BUILDFLAG(IS_CHROMEOS)
  void ReadGroup();
  void ReadIcon();
#endif
#if BUILDFLAG(IS_WIN)
  // Reads an icon in a sandboxed service. Use this when the file itself must
  // be parsed.
  void ReadIconInSandbox();
#endif

  // The traits of the tasks posted to base::ThreadPool by this class. These
  // operations may block, because they are fetching icons from the disk, yet
  // the result will be seen by the user so they should be prioritized
  // accordingly. They should not however block shutdown if long running.
  static constexpr base::TaskTraits traits() {
    return {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
            base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
  }

  // The task runner object of the thread in which we notify the delegate.
  scoped_refptr<base::SingleThreadTaskRunner> target_task_runner_;

  base::FilePath file_path_;

  IconGroup group_;

#if !BUILDFLAG(IS_ANDROID)
  IconSize icon_size_;
#endif  // !BUILDFLAG(IS_ANDROID)
  const float scale_;
  IconLoadedCallback callback_;
};

#endif  // CHROME_BROWSER_ICON_LOADER_H_
