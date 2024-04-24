// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_transcoder/svg_icon_transcoder.h"

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace apps {

namespace {

constexpr char kSvgDataUrlPrefix[] = "data:image/svg+xml;base64,";

std::string ReadSvgOnFileThread(base::FilePath svg_path) {
  std::string svg_data;
  if (base::PathExists(svg_path)) {
    base::ReadFileToString(svg_path, &svg_data);
    LOG_IF(ERROR, svg_data.empty()) << "Empty svg data at path " << svg_path;
  }
  return svg_data;
}

void SaveIconOnFileThread(const base::FilePath& icon_path,
                          const std::string& content) {
  DCHECK(!content.empty());

  base::File::Error file_error;
  if (!base::CreateDirectoryAndGetError(icon_path.DirName(), &file_error)) {
    LOG(ERROR) << "Failed to create dir " << icon_path.DirName()
               << " with error " << file_error;
    return;
  }

  if (!base::WriteFile(icon_path, content)) {
    LOG(ERROR) << "Failed to write icon file: " << icon_path;
    if (!base::DeleteFile(icon_path)) {
      LOG(ERROR) << "Couldn't delete broken icon file" << icon_path;
    }
  }
}

}  // namespace

SvgIconTranscoder::SvgIconTranscoder(content::BrowserContext* context)
    : browser_context_(context) {}

SvgIconTranscoder::~SvgIconTranscoder() {
  RemoveObserver();
}

// Reads the svg data at svg_path and invokes the string Transcode method.
// |callback| is invoked with and empty string on failure. Blocking call.
void SvgIconTranscoder::Transcode(const base::FilePath&& svg_path,
                                  const base::FilePath&& png_path,
                                  gfx::Size preferred_size,
                                  IconContentCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReadSvgOnFileThread, std::move(svg_path)),
      base::BindOnce(
          [](base::WeakPtr<SvgIconTranscoder> weak_this,
             const base::FilePath&& png_path, gfx::Size preferred_size,
             IconContentCallback callback, std::string svg_data) {
            if (weak_this && !svg_data.empty()) {
              weak_this->Transcode(std::move(svg_data), std::move(png_path),
                                   preferred_size, std::move(callback));
              return;
            }

            std::move(callback).Run(std::string());
          },
          GetWeakPtr(), std::move(png_path), preferred_size,
          std::move(callback)));
}

// Validates and trims the svg_data before base64 encoding and dispatching to
// |web_contents_| in a data: URI.  |callback| is invoked with and empty
// string on failure. Blocking call.
void SvgIconTranscoder::Transcode(const std::string& svg_data,
                                  const base::FilePath&& png_path,
                                  gfx::Size preferred_size,
                                  IconContentCallback callback) {
  if (!PrepareWebContents()) {
    LOG(ERROR) << "Can't transcode svg. WebContents not ready.";
    std::move(callback).Run(std::string());
    return;
  }

  auto pos = svg_data.find("<svg");
  if (pos == std::string::npos) {
    LOG(ERROR) << "Invalid data. Couldn't find <svg.";
    std::move(callback).Run(std::string());
    return;
  }
  // Form a data: uri from the svg_data starting at the <svg. Excess ASCII
  // whitespace is also removed.
  std::string base64_svg = base::Base64Encode(
      base::CollapseWhitespaceASCII(svg_data.substr(pos), false));

  GURL data_url(kSvgDataUrlPrefix + base64_svg);

  web_contents_->DownloadImage(
      data_url, /*is_favicon=*/false, preferred_size, /*max_bitmap_size=*/0,
      /*bypass_cache=*/true,
      base::BindOnce(&SvgIconTranscoder::OnDownloadImage, GetWeakPtr(),
                     std::move(png_path), std::move(callback)));
}

void SvgIconTranscoder::MaybeCreateWebContents() {
  if (!web_contents_) {
    auto params = content::WebContents::CreateParams(browser_context_);
    params.initially_hidden = true;
    params.desired_renderer_state =
        content::WebContents::CreateParams::kInitializeAndWarmupRendererProcess;
    web_contents_ = content::WebContents::Create(params);
    // When we observe RenderProcessExited, we will need to recreate.
    web_contents_->GetPrimaryMainFrame()->GetProcess()->AddObserver(this);
  }
}

bool SvgIconTranscoder::PrepareWebContents() {
  if (!web_contents_ready_) {
    // Old web_contents_ may have been destroyed.
    MaybeCreateWebContents();
    if (web_contents_->GetPrimaryMainFrame()->IsRenderFrameLive()) {
      web_contents_ready_ = true;
    }
    VLOG(1) << "web_contents "
            << (web_contents_ready_ ? "ready " : "not ready");
  }
  return web_contents_ready_;
}

void SvgIconTranscoder::RenderProcessReady(content::RenderProcessHost* host) {
  web_contents_ready_ = true;
}

void SvgIconTranscoder::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  web_contents_ready_ = false;
  RemoveObserver();
  web_contents_.reset();
}

void SvgIconTranscoder::RemoveObserver() {
  if (web_contents_ && web_contents_->GetPrimaryMainFrame()) {
    web_contents_->GetPrimaryMainFrame()->GetProcess()->RemoveObserver(this);
  }
}

// Compresses the first received bitmap and  saves compressed data to
// |png_path| if non-empty. If the file can't be saved, that's not considered
// and error. Next time lucky.
void SvgIconTranscoder::OnDownloadImage(base::FilePath png_path,
                                        IconContentCallback callback,
                                        int id,
                                        int http_status_code,
                                        const GURL& image_url,
                                        const std::vector<SkBitmap>& bitmaps,
                                        const std::vector<gfx::Size>& sizes) {
  if (bitmaps.empty()) {
    VLOG(1) << "status " << http_status_code << " for download id " << id;
    VLOG(1) << "Failed to download image from " << image_url;
    std::move(callback).Run(std::string());
    return;
  }

  const SkBitmap& bitmap = bitmaps[0];

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](const SkBitmap& bitmap) {
            std::vector<unsigned char> compressed;
            if (!bitmap.empty() &&
                gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &compressed)) {
              return std::string(compressed.begin(), compressed.end());
            }
            return std::string();
          },
          bitmap),
      base::BindOnce(
          [](base::FilePath png_path, IconContentCallback callback,
             std::string compressed) {
            if (!compressed.empty() && !png_path.empty()) {
              base::ThreadPool::PostTaskAndReply(
                  FROM_HERE,
                  {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
                  base::BindOnce(&SaveIconOnFileThread, std::move(png_path),
                                 compressed),
                  base::BindOnce(std::move(callback), compressed));
            } else {
              std::move(callback).Run(std::move(compressed));
            }
          },
          std::move(png_path), std::move(callback)));
}

}  // namespace apps
