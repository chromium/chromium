# Testing Chromium with AutofillServices

[TOC]

Autofill Services provide Autofill data for all apps. In Chromium, a built-in
service provides Autofill data across all platforms. Users can switch to using
the device-wide Autofill Service. This example implementation is used to test
Chromium with a primitive implementation.

## What does this Autofill Service do?

It provides an AutofillService which provides static data for each field by
echoing the type of field and a number. It allows filling simple forms.
This Service supports inline suggestions.

The main activity explains how to set the service in settings and provides
additional information.

It should help to understand how an app can interact with Chrome using intents
and ContentProviders.

## Building

These instruction assume that you have already built Chromium for Android. If
not, instructions for building Chromium for Android are
[here](/docs/android_build_instructions.md). Details below assume that the
build is setup in `$CHROMIUM_OUTPUT_DIR`.

### Build the Chromium Test Autofill App

To build the test app and the AutofillService, execute:

```shell
$ autoninja -C $CHROMIUM_OUTPUT_DIR inline_autofill_service_example_apk
```

### Install the Chromium Test Autofill App

To install the test app and the AutofillService, execute:

```shell
# Install the example
$ $CHROMIUM_OUTPUT_DIR/bin/inline_autofill_service_example_apk install
```

## Usage

### Using the Autofill Service

The Autofill Service has to be enabled in Android settings. Select it as
Autofill provider.
Additionally, enable in Chromium settings > Autofill Services that the external
service may be used. Restart Chromium.

### Using the Test App

The test app appears in the app drawer and can be started from there.
