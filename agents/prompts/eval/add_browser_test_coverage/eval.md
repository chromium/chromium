* Owners: jonathanjlee@google.com
* Description: Generate a browser test from a description and existing tests.
* Git-Revision: 4ea660843265a6544c61cc262e3efbc70869c2a4
* Result: Test successfully compiles and passes. At minimum, the added code
  should:
  * Be contained within one `IN_PROC_BROWSER_TEST_F(WebUsbTest, ...) {...}`,
    where the case name contains `Open` or `Close`.
  * Always `EvalJs()` or `ExecJs()` syntactically correct JavaScript against the
    current `web_contents()`.
  * Contain three Googletest assertions in order:
    1. `EXPECT_TRUE(EvalJs(...))` (or equivalent) for the first `opened` check
    1. `EXPECT_FALSE(EvalJs(...))` for the second `opened` check
    1. `EXPECT_EQ(ListValueOf("123456"), EvalJs(...))` to verify the device is
       still recognized
  * Contain the following JavaScript substrings in order, across all `EvalJs()`
    or `ExecJs()` calls:
    1. `navigator.usb.requestDevice({ filters: [{ vendorId: 0 }] })` (for
       granting permission to the fake device)
    1. `.open()`
    1. `.opened`
    1. `.close()`
    1. `.opened`
    1. `navigator.usb.getDevices()`
* Modified files:
  * `content/browser/usb/usb_browsertest.cc`

Sample test:

```
diff --git a/content/browser/usb/usb_browsertest.cc b/content/browser/usb/usb_browsertest.cc
index db91960bc0c1c..e9474bc9aeb8f 100644
--- a/content/browser/usb/usb_browsertest.cc
+++ b/content/browser/usb/usb_browsertest.cc
@@ -235,6 +235,39 @@ IN_PROC_BROWSER_TEST_F(WebUsbTest, ForgetDevice) {
       })())"));
 }

+IN_PROC_BROWSER_TEST_F(WebUsbTest, OpenClose) {
+  // Request permission to access the fake device.
+  EXPECT_EQ("123456", EvalJs(web_contents(),
+                             R"((async () => {
+        let device =
+            await navigator.usb.requestDevice({ filters: [{ vendorId: 0 }] });
+        return device.serialNumber;
+      })())"));
+
+  // Get the device and open it.
+  EXPECT_EQ(true, EvalJs(web_contents(),
+                         R"((async () => {
+        let devices = await navigator.usb.getDevices();
+        await devices[0].open();
+        return devices[0].opened;
+      })())"));
+
+  // Close the device.
+  EXPECT_EQ(false, EvalJs(web_contents(),
+                          R"((async () => {
+        let devices = await navigator.usb.getDevices();
+        await devices[0].close();
+        return devices[0].opened;
+      })())"));
+
+  // Check that the device is still in the getDevices() array.
+  EXPECT_EQ(ListValueOf("123456"), EvalJs(web_contents(),
+                                          R"((async () => {
+        let devices = await navigator.usb.getDevices();
+        return devices.map(d => d.serialNumber);
+      })())"));
+}
+
 }  // namespace

 }  // namespace content
```
