# Touch To Fill Android Feature

This folder contains the Android UI implementation for the Touch To Fill
feature. Touch To Fill provides users with a trusted surface to authorize
transactions, such as filling their credentials.

[TOC]

## Use case

This component displays a set of saved credentials. The user selects one which
is then filled into the form. If the user dismisses the sheet, the keyboard
will be shown instead (i.e. by changing the focus).


## Folder Structure

#### java/

The root folder contains the public interface of this component and data that is
used to fill it with content, e.g. Crendentials. This folder also contains the
factory to instantiate the component. The factory has two implementations:

* an auto-generated one returning null which is used in tests and targets that
  don't directly depend on this component
* one that provides the actual implementation

Add `chrome/browser/touch_to_fill/android:public_java` as dependency to use the
interface and classes defined here.

#### java/internal/

Contains the actual implementation. Don't try to use any class defined here
outside of this package. If you need access to any method, consider making it
part of the public interface as defined in `TouchToFillComponent`

This folder contains a separate [README](internal/README.md) that explains in
detail how the architecture looks like and how to extend the component further.

#### junit/

Contains Robolectric tests that test the controller of the component. These
tests run without emulator which means that they are incredibly fast but cannot
instantiate or verify Android Views.
The public build and run target is `touch_to_fill_junit_tests`. Run them with:

``` bash
./out/<OutDirectory>/bin/run_touch_to_fill_junit_tests
```

Although the entire suite should run in seconds, you can filter tests with `-f`.


#### javatests/

Contains View and Integration tests. These tests need an emulator which means
that they run slowly but can test rendered Views. Native Calls should be used
very sparingly and only on Lollipop and higher to ensure access to all methods
in the Dex path list.
The public build and run target is `chrome_public_test_apk`. Run them with:

``` bash
./out/<OutDirectory>/bin/run_chrome_public_test_apk -f *TouchToFill*
```

## Example usage

``` java

// Currently, you need access to internal/ to instantiate the component:
TouchToFillComponent component = new TouchToFillCoordinator(/*...*/);

component.initialize(activity, activity.getBottomSheetController(), () -> {
  // Things to do when the component is dismissed.
}));

List<Credential> creds; // Add credentials to show!
component.showCredentials("www.displayed-url.xzy", creds, (credential) -> {
  // The |credential| that was clicked should be used to fill something now.
})

```
