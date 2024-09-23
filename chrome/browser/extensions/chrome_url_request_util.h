// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_URL_REQUEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_URL_REQUEST_UTIL_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace base {
class FilePath;
}

namespace net {
class HttpResponseHeaders;
}

namespace network {
struct ResourceRequest;
}

namespace extensions {
class Extension;
class ExtensionSet;
class ProcessMap;

// Utilities related to URLRequest jobs for extension resources. See
// chrome/browser/extensions/extension_protocols_unittest.cc for related tests.
namespace chrome_url_request_util {

// Sets allowed=true to allow a chrome-extension:// resource request coming from
// renderer A to access a resource in an extension running in renderer B.
// Returns false when it couldn't determine if the resource is allowed or not
bool AllowCrossRendererResourceLoad(
    const network::ResourceRequest& request,
    network::mojom::RequestDestination destination,
    ui::PageTransition page_transition,
    int child_id,
    bool is_incognito,
    const Extension* extension,
    const ExtensionSet& extensions,
    const ProcessMap& process_map,
    const GURL& upstream_url,
    bool* allowed);

// Return the |request|'s resource path relative to the Chromium resources path
// (chrome::DIR_RESOURCES) *if* the request refers to a resource within the
// Chrome resource bundle. If not then the returned file path will be empty.
// |resource_id| is used to check whether the requested resource is registered
// as a component extensions resource, via
// ChromeComponentExtensionResourceManager::IsComponentExtensionResource()
base::FilePath GetBundleResourcePath(
    const network::ResourceRequest& request,
    const base::FilePath& extension_resources_path,
    int* resource_id);

// Creates and starts a URLLoader for loading component extension resources out
// of a Chrome resource bundle. This should only be called if
// GetBundleResourcePath returns a valid path.
void LoadResourceFromResourceBundle(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    const base::FilePath& resource_relative_path,
    int resource_id,
    scoped_refptr<net::HttpResponseHeaders> headers,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client);

}  // namespace chrome_url_request_util
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_URL_REQUEST_UTIL_H_
