# Graph View

The graph view displays an animated graph of the resource coordinator graph.
This graph is a course-level overview of the browser's state, broken down
into pages, each of which has a frame tree. Each frame is then hosted by a
process.

## Rendering
To render and animate the graph, the implementation relies on
[D3.js](https://d3js.org), which transforms the graph to SVG DOM elements.
The animation is the [D3 force layout](https://github.com/d3/d3-force) using
X and Y attracting forces to pull pages to the top, processes to the bottom,
and the graph at large to the center.

Because D3.js is rather large, it was deemed unreasonable to ship it as part of
Chrome for occasional use by this debug page. For this reason, D3.js is loaded
from an external, Google controlled page.

## Security concerns
WebUI runs in privileged renderer processes, each of which has access to the set
of APIs exposed by their corresponding WebUIController. In the case of the
discards UI, this includes the DiscardsDetailsProvider interface. This allows
discarding pages, as well as e.g. querying and manipulating the browser state.

The D3 library is loaded from the web, and it was strongly desired not to load
it into a WebUI renderer. Instead, the graph rendering takes place in a
`<webview>`, which is hosted in a second, unprivileged renderer process.
This limits Chrome's exposure in case of implementation error in the D3 library
exploitable errors in the renderer.

The main body document of this webview is provided in a data URL.
To ensure that the D3.js library is not modified in transport it is pinned with
[subresource integrity](https://developer.mozilla.org/en-US/docs/Web/Security/Subresource_Integrity).
This is mainly to ensure that the graph view is not subverted, as it handles
potentially sensitive data, like the URLs currently loaded.

Communication to the webview is done with one-way postMessage, from WebUI to
webview.

## Incidental Detail
In order to allow type checking the main body TypeScript code with the
TypeScript compiler, it is hosted in a separate file named `graph_doc.ts`. At
build time, the TypeScript code is merged into the `graph_doc_template.html`
file, and the whole lot is encoded in a data URL, which is merged into the
`graph_tab.html` file.

## How to debug
To debug the contents in the webview, navigate to the development tools
page inspector (`chrome://inspect/#pages`), and select the inspect link from the
subpage you see there under the `chrome://discards` page.
