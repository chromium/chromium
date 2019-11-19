# Android WebView Resources

This folder is responsible for managing WebView's application resources. Please
also consult [general UI/localization information][1] for the chromium
repository.

## Adding a WebView-specific string/resource

The process is similar to the [general instructions][1], with the caveat that
strings should be added to the WebView-specific GRD file
([`aw_strings.grd`](./aw_strings.grd), translations live in
[translations/](./translations/)).

WebView-specific file resources should be declared in
[`aw_resources.grd`](./aw_resources.grd), and the file templates should live
under [resources/](./resources/)).

No `<if>` clause is generally necessary: it's already implied these are for
WebView builds only.

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

*** note
**Note:** WebView-specific resources are prefixed by the
`org.chromium.android_webview` package name.
***

## Shared/common resources

WebView can use strings and resources defined in GRD files in other layers (for
now, only `//components/`). Unlike other Chrome, we trim out all such resources
by default. To use these resources, you must add the resource to the appropriate
whitelist file: [`grit_strings_whitelist.txt`](./grit_strings_whitelist.txt) for
`IDS_*` strings or
[`grit_resources_whitelist.txt`](./grit_resources_whitelist.txt) for `IDR_*`
resources.

*** note
**Note:** Inflating a
non-whitelisted resource triggers a `DCHECK` (in release builds, this usually
inflates to empty content).
***

[1]: http://www.chromium.org/developers/design-documents/ui-localization
