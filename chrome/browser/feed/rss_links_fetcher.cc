// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/rss_links_fetcher.h"

#include "base/functional/callback.h"
#include "components/feed/mojom/rss_link_reader.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace feed {
namespace {

mojo::Remote<feed::mojom::RssLinkReader> GetRssLinkReaderRemote(
    content::WebContents* web_contents) {
  DCHECK(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());
  mojo::Remote<feed::mojom::RssLinkReader> result;
  // GetRemoteInterfaces() cannot be null if the render frame is created.
  web_contents->GetPrimaryMainFrame()->GetRemoteInterfaces()->GetInterface(
      result.BindNewPipeAndPassReceiver());
  return result;
}

class RssLinksFetcher {
 public:
  void Start(const GURL& page_url,
             mojo::Remote<feed::mojom::RssLinkReader> link_reader,
             base::OnceCallback<void(std::vector<GURL>)> callback) {
    page_url_ = page_url;
    callback_ = std::move(callback);
    link_reader_ = std::move(link_reader);
    if (link_reader_) {
      // Unretained is OK here. The `mojo::Remote` will not invoke callbacks
      // after it is destroyed.
      link_reader_.set_disconnect_handler(base::BindOnce(
          &RssLinksFetcher::SendResultAndDeleteSelf, base::Unretained(this)));
      link_reader_->GetRssLinks(base::BindOnce(
          &RssLinksFetcher::GetRssLinksComplete, base::Unretained(this)));
      return;
    }
    SendResultAndDeleteSelf();
  }

 private:
  void GetRssLinksComplete(feed::mojom::RssLinksPtr rss_links) {
    if (rss_links) {
      if (rss_links->page_url == page_url_) {
        for (GURL& link : rss_links->links) {
          if (link.is_valid() && link.SchemeIsHTTPOrHTTPS()) {
            result_.push_back(std::move(link));
          }
        }
      }
    }

    SendResultAndDeleteSelf();
  }
  void SendResultAndDeleteSelf() {
    std::move(callback_).Run(std::move(result_));
    delete this;
  }

  GURL page_url_;
  mojo::Remote<feed::mojom::RssLinkReader> link_reader_;
  std::vector<GURL> result_;
  base::OnceCallback<void(std::vector<GURL>)> callback_;
};

void FetchRssLinksHelper(const GURL& url,
                         mojo::Remote<feed::mojom::RssLinkReader> link_reader,
                         base::OnceCallback<void(std::vector<GURL>)> callback) {
  // RssLinksFetcher is self-deleting.
  auto* fetcher = new RssLinksFetcher();
  fetcher->Start(url, std::move(link_reader), std::move(callback));
}

}  // namespace

void FetchRssLinksForTesting(
    const GURL& url,
    mojo::Remote<feed::mojom::RssLinkReader> link_reader,
    base::OnceCallback<void(std::vector<GURL>)> callback) {
  FetchRssLinksHelper(url, std::move(link_reader), std::move(callback));
}

void FetchRssLinks(const GURL& url,
                   content::WebContents* web_contents,
                   base::OnceCallback<void(std::vector<GURL>)> callback) {
  if (!web_contents) {
    std::move(callback).Run(std::vector<GURL>());
    return;
  }
  FetchRssLinksHelper(url, GetRssLinkReaderRemote(web_contents),
                      std::move(callback));
}

}  // namespace feed
