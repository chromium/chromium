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

> Note: See additional instructions on how to iterate faster using the
  [--load-webui-from-disk flow](../../../../docs/webui/webui_in_chrome.md#Load-WebUIs-straight-from-disk-experimental_)
