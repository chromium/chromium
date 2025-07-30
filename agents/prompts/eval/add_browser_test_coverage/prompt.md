First, read the following files to familiarize yourself with browser test APIs:
* `//content/public/test/browser_test_base.h`
* `//content/public/test/browser_test_utils.h`
* `//content/public/test/content_browser_test.h`

Next, read the following MDN articles to familiarize yourself with WebUSB, a
JavaScript API for interacting with USB devices:
* https://developer.mozilla.org/en-US/docs/Web/API/WebUSB_API
* https://developer.mozilla.org/en-US/docs/Web/API/USB
* https://developer.mozilla.org/en-US/docs/Web/API/USBDevice

Finally, add a test case to `//content/browser/usb/usb_browsertest.cc` that
tests the following scenario:
1. Open the mock device with `open()`
1. Check that the device's `opened` attribute is `true`
1. Close the mock device with `close()`
1. Check that the device's `opened` attribute is now `false`
1. Check that the device is still in the array returned by
   `navigator.usb.getDevices()`

Once the test is written, verify the test passes with `autotest.py`.

There's no need to modify any `.gn` files because `usb_browsertest.cc` is an
existing source file.
