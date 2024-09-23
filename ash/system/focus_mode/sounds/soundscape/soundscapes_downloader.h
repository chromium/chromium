// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_SOUNDSCAPE_SOUNDSCAPES_DOWNLOADER_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_SOUNDSCAPE_SOUNDSCAPES_DOWNLOADER_H_

#include <optional>
#include <string>
#include <utility>

#include "ash/ash_export.h"
#include "ash/system/focus_mode/sounds/soundscape/soundscape_types.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

// Downloads the configuration for the Soundscapes feature (playlists, tracks,
// and thumbnail data) from the server and returns the parsed result to the
// caller via callback.
class ASH_EXPORT SoundscapesDownloader {
 public:
  struct Urls {
    Urls();
    Urls(const Urls&);
    ~Urls();

    std::string locale;
    // URL root for soundscape resources.
    GURL host;
    // Path of the configuration json relative to `host`.
    std::string config_path;
  };

  static std::unique_ptr<SoundscapesDownloader> Create(
      const std::string& locale);
  static std::unique_ptr<SoundscapesDownloader> CreateForTesting(
      const Urls& origins,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  virtual ~SoundscapesDownloader() = default;

  // Returns the result of the request for the configuration. If the downloaded
  // configuration is valid, the parsed representation is provided. If the
  // download failed or the file failed to validate, nullopt is provided.
  using ConfigurationCallback =
      base::OnceCallback<void(std::optional<SoundscapeConfiguration>)>;

  // Starts the request for the configuration file. `callback` is invoked when
  // the file is retrieved or we've given up.
  virtual void FetchConfiguration(ConfigurationCallback callback) = 0;

  // Returns a `GURL` rooted at the configured host for `path`.
  virtual GURL ResolveUrl(std::string_view path) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_SOUNDSCAPE_SOUNDSCAPES_DOWNLOADER_H_
