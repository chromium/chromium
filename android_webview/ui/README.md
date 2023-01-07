# Android WebView Resources

This folder is responsible for managing WebView's application resources. Please
also consult [general UI/localization information][1] for the chromium
repository.

> **Note:** The original grd files located here had no content and were removed
in [crrev/c/3606985](https://crrev.com/c/3606985).

## Adding a WebView-specific Android/Java string

The process is again similar to the [general instructions][1]. Use
[`//android_webview/java/strings/android_webview_strings.grd`](/android_webview/java/strings/android_webview_strings.grd).
A string `IDS_MY_STRING` can be accessed in Java with
`org.chromium.android_webview.R.string.MY_STRING`.

Resources are added under
[`//android_webview/java/res/`](/android_webview/java/res/). Similarly, a
resource `drawable-xxxhdpi/ic_play_circle_outline_black_48dp.png` can be
accessed in Java with
`org.chromium.android_webview.R.drawable.ic_play_circle_outline_black_48dp`.

> **Note:** WebView-specific resources are prefixed by the
`org.chromium.android_webview` package name.

## Shared/common resources

WebView can use strings and resources defined in GRD files in other layers (for
now, only `//components/`). Unlike other Chrome, we trim out all such resources
by default. To use these resources, you must add the resource to the appropriate
allowlist file: [`grit_strings_allowlist.txt`](./grit_strings_allowlist.txt) for
`IDS_*` strings or
[`grit_resources_allowlist.txt`](./grit_resources_allowlist.txt) for `IDR_*`
resources.

> **Note:** Inflating a non-allowlisted resource triggers a `DCHECK`
(in release builds, this usually inflates to empty content).

[1]: http://www.chromium.org/developers/design-documents/ui-localization
