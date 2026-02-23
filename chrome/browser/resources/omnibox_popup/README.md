# WebUI Omnibox Popup experiment

## Debug instructions

* Launch Chrome with `--enable-features=WebUIOmniboxPopupDebug` or enable it via
`chrome://flags/#webui-omnibox-popup-debug`.
  * For side-by-side comparison with the Views popup, launch Chrome with
  `--enable-features=WebUIOmniboxPopupDebug:SxS/true` or select the
  `Side by Side` flag variant.
* Navigate to `chrome://omnibox-popup.top-chrome?debug` in a tab. A blank page
  should be shown.
* Start typing a query into the omnibox. The omnibox suggestions should be shown
  within the tab.
* Blur the omnibox for the real popup to close and see only the results
  in the tab.
* Open DevTools (right click -> Inspect) to debug as usual.

### Alternative debug instructions for the omnibox popup

The above instructions are convenient for debugging in a tab, but it may
sometimes be helpful to debug the omnibox popup itself instead of a
secondary webui surface.

- Open chrome://inspect/#pages
- Press `inspect` for chrome://omnibox-popup.top-chrome/ to debug the main
  browser window's actual omnibox popup. Note, each browser window creates an
  identically-named entry on the list (and if the DevTools are popped out to a
  separate window, that may appear as well) so to be sure of inspecting the
  right omnibox, start with a single window.
- DevTools may not show source code for the webui omnibox (e.g. app.js) so
  focus the DevTools window and press Ctrl+Shift+R to hard reload. Then the
  source code appears and the omnibox in the browser window can be debugged
  directly. For example a breakpoint can be set and then triggered by typing
  into the omnibox itself.

> Note: See additional instructions on how to iterate faster using the
  [--load-webui-from-disk flow](../../../../docs/webui/webui_in_chrome.md#Load-WebUIs-straight-from-disk-experimental_)
