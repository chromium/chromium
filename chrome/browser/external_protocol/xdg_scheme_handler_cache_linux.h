// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTERNAL_PROTOCOL_XDG_SCHEME_HANDLER_CACHE_LINUX_H_
#define CHROME_BROWSER_EXTERNAL_PROTOCOL_XDG_SCHEME_HANDLER_CACHE_LINUX_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"

// Tracks which URL schemes have an application registered for them with the
// desktop, i.e. an "x-scheme-handler/<scheme>" association as defined by the
// XDG MIME Applications specification. This is what xdg-open (and the desktop
// helpers it calls) use to pick a handler, so a scheme in this set is expected
// to open in an external application.
//
// The association files are read on a background sequence and watched for
// changes, so `HasHandler()` never blocks. Until the first scan finishes,
// `HasHandler()` returns false.
//
// Instances are shared: `GetOrCreate()` returns the live instance if there is
// one. Must be used on a single sequence (the UI thread).
class XdgSchemeHandlerCache : public base::RefCounted<XdgSchemeHandlerCache> {
 public:
  // Returns the live cache, or creates one and starts the initial scan.
  static scoped_refptr<XdgSchemeHandlerCache> GetOrCreate();

  XdgSchemeHandlerCache(const XdgSchemeHandlerCache&) = delete;
  XdgSchemeHandlerCache& operator=(const XdgSchemeHandlerCache&) = delete;

  // Returns whether `scheme` (compared case-insensitively) has a registered
  // handler, according to the last completed scan.
  bool HasHandler(std::string_view scheme) const;

  // Runs `callback` after every completed scan.
  void SetOnUpdatedCallbackForTesting(base::RepeatingClosure callback);

  // Scans the XDG MIME association files and returns the (lowercase) schemes
  // that have at least one installed handler. `config_dirs` (e.g.
  // ~/.config, /etc/xdg) and `applications_dirs` (e.g.
  // ~/.local/share/applications, /usr/share/applications) are in decreasing
  // order of precedence. `desktops` are the lowercase names from
  // $XDG_CURRENT_DESKTOP. Relative dirs are ignored. Blocking.
  static base::flat_set<std::string> ScanForSchemeHandlers(
      const std::vector<base::FilePath>& config_dirs,
      const std::vector<base::FilePath>& applications_dirs,
      const std::vector<std::string>& desktops);

 private:
  friend class base::RefCounted<XdgSchemeHandlerCache>;
  class Backend;

  XdgSchemeHandlerCache();
  ~XdgSchemeHandlerCache();

  void OnScanned(base::flat_set<std::string> schemes);

  SEQUENCE_CHECKER(sequence_checker_);

  base::flat_set<std::string> schemes_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::RepeatingClosure on_updated_for_testing_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::SequenceBound<Backend> backend_;
  base::WeakPtrFactory<XdgSchemeHandlerCache> weak_factory_{this};
};

#endif  // CHROME_BROWSER_EXTERNAL_PROTOCOL_XDG_SCHEME_HANDLER_CACHE_LINUX_H_
