# Password Check Android Feature

This folder contains the Android UI implementation for the Password Check
feature. Password Check provides users with an overview of compromised
credentials and options to fix them.

[TOC]

## Use case

This component displays a set of compromised credentials. The user can fight any
potential misuse by changing affected credentials (either for a particular site
or on all pages). The site will support different kinds of compromised
credentials.


## Folder Structure

#### java/

The root folder contains the public interface of this component and data that is
used by other components (like the settings fragment). This folder also contains
the factory to instantiate the component. The factory has two implementations:

* an auto-generated one returning null which is used in tests and targets that
  don't directly depend on this component
* one that provides the actual implementation

Add `chrome/browser/password_check/android:public_java` as dependency to use the
interface and classes defined here.

#### java/internal/

Contains the actual implementation. Don't try to use any class defined here
outside of this package. If you need access to any method, consider making it
part of the public interface as defined in `PasswordCheckComponent`

This folder contains a separate [README](internal/README.md) that explains in
detail how the architecture looks like and how to extend the component further.

#### junit/

Contains Robolectric tests that test the controller of the component. These
tests run without emulator which means that they are incredibly fast but cannot
instantiate or verify Android Views.
The public build and run target is `password_check_junit_tests`. Run them with:

``` bash
./out/<OutDirectory>/bin/run_password_check_junit_tests
```

Although the entire suite should run in seconds, you can filter tests with `-f`.


#### javatests/

Contains View and Integration tests. These tests need an emulator which means
that they run slowly but can test rendered Views.
The public build and run target is `chrome_public_test_apk`. Run them with:

``` bash
./out/<OutDirectory>/bin/run_chrome_public_test_apk -f *PasswordCheck*
```

## Example usage

``` java

// Currently, you need access to internal/ to instantiate the component:
PasswordCheckComponent component =
    new PasswordCheckComponentFactory.createComponent(/*...*/);

component.initialize(activity);

```
