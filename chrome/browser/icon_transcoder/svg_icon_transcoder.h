// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ICON_TRANSCODER_SVG_ICON_TRANSCODER_H_
#define CHROME_BROWSER_ICON_TRANSCODER_SVG_ICON_TRANSCODER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace apps {

using IconContentCallback = base::OnceCallback<void(std::string)>;

// SvgIconTranscoder uses WebContents to transform an svg icon (as a file or
// as string data) into an SkBitmap and thence into png compressed data which
// can be written to a png file. In principal, this technique should work for
// any data:image/ mime-types supported by WebContents, but svg is all we need
// right now. File handling happens in the browser process.
// Some validation of svg data is performed prior to asking a WebContents
// renderer process (which could potentially die on bad data) to render the
// image. Since a renderer process can be destroyed for many valid reasons,
// SvgIconTranscoder always checks if its WebContents must be recreated.
class SvgIconTranscoder : public content::RenderProcessHostObserver {
 public:
  explicit SvgIconTranscoder(content::BrowserContext* context);

  SvgIconTranscoder(const SvgIconTranscoder&) = delete;
  SvgIconTranscoder& operator=(const SvgIconTranscoder&) = delete;
  ~SvgIconTranscoder() override;

  // Reads the svg data at svg_path and invokes the string Transcode method.
  // |callback| is invoked with and empty string on failure. Blocking call.
  void Transcode(const base::FilePath&& svg_path,
                 const base::FilePath&& png_path,
                 gfx::Size preferred_size,
                 IconContentCallback callback);

  // Validates and trims the svg_data before base64 encoding and dispatching to
  // |web_contents_| in a data: URI.  |callback| is invoked with and empty
  // string on failure. Blocking call.
  void Transcode(const std::string& svg_data,
                 const base::FilePath&& png_path,
                 gfx::Size preferred_size,
                 IconContentCallback callback);

  base::WeakPtr<SvgIconTranscoder> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void MaybeCreateWebContents();

  bool PrepareWebContents();

  void RemoveObserver();

  // content::RenderProcessHostObserver:
  void RenderProcessReady(content::RenderProcessHost* host) override;
  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;

  // Compresses the first received bitmap and  saves compressed data to
  // |png_path| if non-empty. If the file can't be saved, that's not considered
  // and error. Next time lucky.
  void OnDownloadImage(base::FilePath png_path,
                       IconContentCallback callback,
                       int id,
                       int http_status_code,
                       const GURL& image_url,
                       const std::vector<SkBitmap>& bitmaps,
                       const std::vector<gfx::Size>& sizes);

  const raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  bool web_contents_ready_{false};
  base::WeakPtrFactory<SvgIconTranscoder> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_ICON_TRANSCODER_SVG_ICON_TRANSCODER_H_
