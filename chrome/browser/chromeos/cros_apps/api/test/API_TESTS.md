# Writing ChromeOS Apps API Tests

[TOC]

## Overview
This document provides an overview of the types of tests that can be used for
ChromeOS App API implementations as well as general guidance about when to use
which type of test.


More specifically, this guidance focuses on testing ChromeOS App APIs in an
end-to-end fashion, starting from JavaScript calls down to a reasonable layer of
abstraction for that API. These types of tests have proven effective for testing
similar types of multi-layered JavaScript APIs i.e. Extension APIs and Web APIs.

## Survey of Test Types

### Common Test Types

#### Unit Tests
Unit tests in Chromium refer to tests that run in a single process. This process
may be the browser process (the main “Chrome” process), a renderer process (such
as a website process), or a utility process (such as one used to parse untrusted
JSON). Unit tests in Chromium can be multi-threaded, but cannot span multiple
processes. Many pieces of the environment are either mocked or stubbed out, or
simply omitted, in unit tests.

Unit tests are generally smaller, faster, and significantly less flaky than
other test types. This results in fewer tests getting disabled. However, unit
tests have two significant drawbacks:
* First, since they run in a single process, they are incompatible with anything
that requires both a renderer and a browser, making them less for the end-to-end
tests this document focuses on and is critical for JS APIs.
* Second, because they operate in a significantly pared-down environment, they
may obscure real bugs that can be hit in production.

Build target: `content_unittests`, `unit_tests`, etc

#### Browser Tests
Browser tests in Chromium are multi-process, and instantiate a "real" browser.
That is, the majority of the environment is set up, and it much more closely
resembles an environment that the Chrome browser normally operates in.

Browser tests are useful when a test needs multi-process integration. This is
typically “browser + renderer”, such as when you need to exercise the behavior
of the browser in response to renderer parsing and input (and can’t suitably
mock it out). Browser tests are more expensive (and frequently more flaky, due
to the amount of state and interaction they entail) than unit tests, but also
exercise systems in a more end-to-end fashion, potentially giving more
confidence that something "actually works".

Build target: `content_browsertests`, `browser_tests`, etc

#### Interactive UI Tests
Interactive UI tests are browser tests that execute serially rather than in
parallel. This allows for user interaction and blocking event loops, such as
opening menus, performing click-and-drag events, writing to/reading from
clipboard, etc.

Prefer browser tests over interactive UI tests unless they're necessary, such as
when testing focus, blocking UI, or drag-and-drop interactions.

Build target: `interactive_ui_tests`, etc.

#### Web Tests/Web Platform Tests
Web tests are used by Blink to test many components, including but not limited
to layout, rendering and Web APIs. In general, web tests involve loading pages
in a test renderer and executing the test there.

Most Web Tests use [testharness.js](https://www.w3.org/wiki/Testharness.js).

Web Platform Tests are Web Tests that are shared among all browsers. They help
ensure interoperability between browsers.

Build target: `blink_tests`.

### ChromeOS-specific Browser Tests
ChromeOS tests cover Lacros, Ash, and Platform layers.

In many cases, tests are written so they run when Lacros is enabled and disabled
e.g. the
[Telemetry Extensions API tests](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/chromeos/extensions/telemetry/api/telemetry/telemetry_api_browsertest.cc)
can run as both a [Lacros Browser Test](#Lacros-Browser-Tests)
and an [Ash Browser Test](#Ash-Browser-Tests).

#### Lacros Browser Tests
[Browser Tests](#Browser-Tests) that run in the Lacros binary. We start Ash
once, then start a new Lacros for every test case. crosapi is stubbed out for
these tests, so can only be used if the test doesn’t result in a crosapi call.

Build target: `browser_tests`

#### Ash Browser Tests
[Browser Tests](#Browser-Tests) that run in the Ash binary. They start an Ash
instance for every test case.

These tests run without Lacros enabled i.e. they are meant to test the OS and
Ash Browser work when Lacros is not enabled.

Long term, once Lacros fully launches, many of these tests will be moved to be
Lacros Browser Tests since the Ash Browser will be removed from Ash.

Build target: `browser_tests`

#### Ash Browser Tests that require Lacros
[Browser Tests](#Browser-Tests) with special setup to start a Lacros instance.
These tests run in the ash browser process (browser() will return the Ash
browser), but Lacros is present and running. Useful for when Ash features affect
lacros e.g. tests that a button in ash correctly places lacros windows.

Build target: `browser_tests` but need to be added to a test filter so they run
with the right arguments. See
[this](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/base/chromeos/demo_ash_requires_lacros_browsertest.cc)
example.

#### Lacros Chrome Browser Tests
Similar to [Lacros Browser Tests](#Lacros-Browser-Tests), but crosapi isn’t
stubbed out. Test code runs in the Lacros browser process and browser() returns
a Lacros Browser.

These can be used as integration tests for Lacros features that call into
crosapi or as crosapi unit tests.

In addition to running with a ToT Ash, these tests run with old Ash versions to
test version skew.

Build targets: `lacros_chrome_browsertests`

#### Tast/Crosier Tests
End to end tests that run on real ChromeOS devices. Useful for features that
can’t be tested on ChromeOS on Linux.

Tast tests are written in golang and in a separate repository for Chromium. Not
all Tast tests run as part of the Chromium CQ. Crosier tests, on the other hand,
are written in C++, run in the ash/lacros process and run on the Chromium CQ.

## ChromeOS App API Browser Tests
ChromeOS App API’s version of [Web Tests](#Web-Tests_Web-Platform-Tests). These tests subclass
CrosAppApiBrowserTest and run as Browser Tests. The tests themselves are written
in JavaScript but there’s some C++ to hook up with the existing GTest
infrastructure. These tests use testharness.js which is also used by Web Tests
and Web Platform Tests.

These tests can run either as [Lacros Browser Tests](#Lacros-Browser-Tests),
[Ash Browser Tests](#Ash-Browser-Tests), or
[Lacros Chrome Browser Tests](#Lacros-Chrome-Browser-Tests), depending on what’s
most important to test. See [Test Guidance](#General-Test-Guidance).

## API Implementation Complexity
![API Layers: When Lacros is enabled, Lacros renderer, Lacros Browser, crosapi implementation, Ash API, and ChromeOS Platform. When Lacros is not enabled, Ash Renderer, Ash Browser, Ash API, and ChromeOS Platform.](/docs/images/cros_apps_tests_api_layers.png)

In general, ChromeOS App APIs are implemented across four or five layers:
Lacros/Ash Renderer, Lacros/Ash Browser, crosapi implementation, Ash API, and
ChromeOS platform, e.g. CrOS Healthd.

In many cases, the crosapi implementation and the Ash API will be the same so
the browser-side implementation can use the same code regardless of lacros.

Some of these layers simply forward calls to another layer whereas others will
have complex logic like caching, security checks, validation, filtering,
controlling UI, etc. For example:
* Web Bluetooth: Most implementation complexity is in the Lacros/Ash Browser
i.e. [//content/browser/bluetooth](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/bluetooth/).
The browser process controls UI, stores and checks permissions, caches device
addresses and ids, validates data sent from the renderer, forwards calls from
the renderer to lower level device APIs, forwards calls from the lower level
device APIs to the renderer after performing some filtering. The renderer layer
mostly forwards calls after some basic validation.
* Web USB: Most implementation complexity is in the renderer (1) i.e.
[//third_party/blink/renderer/modules/webusb](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/webusb/).
The renderer validates inputs, dispatches events, forwards calls to the browser,
converts from Mojo types to WebIDL types, etc. Although the browser layer does
some important security checks, it mostly forwards calls to other lower level
APIs.

## General Test Guidance
Because of the multi-layered nature of ChromeOS App API implementations, and
JavaScript API implementations in general, integration style tests have been the
most useful and hence what this guidance focuses on.

That said, this guidance shouldn’t stop you from writing other types of tests
that you think would be useful for your API e.g. unit tests to exercise
individual smaller components, interactive ui tests to test window focus, etc.

What test to write will mostly depend on where the
[API Implementation Complexity](#API-Implementation-Complexity) lies.

As of this writing (2023-12-06), our APIs need to work when Lacros is both
enabled or disabled which should be taken into consideration when choosing one
of the approaches below.

**All test types mentioned below use the ChromeOS App API Test Framework, i.e.
subclass CrosAppApiBrowserTest, unless otherwise specified.**

### Implementation complexity lies mostly in the browser-side implementation of the API

![API Layers: When Lacros is enabled, Lacros renderer, Lacros Browser, and Fake Crosapi Implementation. When Lacros is not enabled, Ash Renderer, Ash Browser, and Fake Ash API.](/docs/images/cros_apps_tests_api_layers_fake_ash.png)

**tldr; a shared [Browser Test](#Browser-Tests) that runs as both, a
[Lacros Browser Test](#Lacros-Browser-Tests) (when Lacros is enabled), and an
[Ash Browser Test](#Ash-Browser-Tests) (when Lacros is not enabled), with the
crosapi / Ash API layer faked.**

Most of the complexity of some APIs will be in the Browser-side implementation
layer which then calls into simple OS APIs (crosapi when Lacros is enabled).

In these cases, you should write a [Browser Test](#Browser-Tests) that can be
run as both a [Lacros Browser Test](#Lacros-Browser-Tests) when Lacros is
enabled and an [Ash Browser Test](#Ash-Browser-Tests when Lacros isn’t enabled.
When Lacros is enabled, fake the crosapi implementation, and when Lacros is
disabled, fake the Ash API. The fake crosapi/API could be controlled in C++
before the test starts, or in JS using Mojo JS.

Once Lacros is fully shipped, the Ash browser test can be removed.

You should consider adding
[Lacros Chrome Browser Tests](#Lacros-Chrome-Browser-Tests) as end-to-end tests
for CUJ, to catch any bugs that could appear when calling into Ash and C++ only
(not API tests) crosapi unit tests to exercise edge cases that can’t be easily
exercised through the JS API.

### Implementation complexity lies mostly in Ash

![API Layers: When Lacros is enabled, Lacros renderer, Lacros Browser, Crosapi Implementation, and Fake Platform. When Lacros is not enabled, Ash Renderer, Ash Browser, Ash API, and Fake Platform.](/docs/images/cros_apps_tests_api_layers_fake_platform.png)

**tldr; a shared [Browser Test](#Browser-Tests) that runs as both a
[Lacros Chrome Browser Test](#Lacros-Chrome-Browser-Tests) and an
[Ash Browser Test](#Ash-Browser-Tests).**

Some API implementations will be simple wrappers around more complex OS APIs
(crosapi when Lacros is enabled).

In these cases, you should write a Browser Test that can be run as both a
[Lacros Chrome Browser Test](#Lacros-Chrome-Browser-Tests) and an
[Ash Browser Test](#Ash-Browser-Tests).

You will have to fake the low level API used by the ChromeOS App API
implementation which will have to be controllable either from Lacros C++ (or Ash
C++ when Lacros is not enabled) or from JS. This can be done by adding a test
interface to
[crosapi.mojom.TestController](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/crosapi/mojom/test_controller.mojom;l=276;drc=ea1ad5d87b9605969600b0808850e072d713385c)
, similar to [ShillClientTestInterface](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/crosapi/mojom/test_controller.mojom;l=416-420;drc=ea1ad5d87b9605969600b0808850e072d713385c)
, which controls the fake implementation in Ash.

Once Lacros is fully shipped, the Ash Browser Test can be removed.

A C++-only crosapi unit test as a
[Lacros Chrome Browser Test](#Lacros-Chrome-Browser-Tests) can also be useful
for exercising edge cases that can’t be easily exercised through the JS API.

### Implementation complexity lies mostly in the Renderer

![API Layers: When Lacros is enabled, Lacros renderer, and Fake Mojo Interface Implementation. When Lacros is not enabled, Ash Rendere and Fake Mojo Interface Implementation.](/docs/images/cros_apps_tests_api_layers_fake_browser.png)

**tldr; a shared [Browser Test](#Browser-Tests) that runs as both, a
[Lacros Browser Test](#Lacros-Browser-Tests) and, an
[Ash Browser Test](#Ash-Browser-Tests), with the Renderer-Browser Mojo interface
faked.**

Few API implementations will have most of their complexity in the renderer and
use simple interfaces to communicate with the Lacros Browser process or Ash
Browser Process.

In these cases, you should write a [Browser Test](#Browser-Tests) that can be
run as both, a [Lacros Browser Test](#Lacros-Browser-Tests) and an
[Ash Browser Test](#Ash-Browser-Tests). The tests should mock/fake the
renderer-browser Mojo interface in JS and exercise the Renderer-side code.

Once Lacros is fully shipped, the Ash Browser Test can be removed.

Adding one or two [Browser Tests](#Browser-Tests), that run as both, a
[Lacros Chrome Browser Test](#Lacros-Chrome-Browser-Tests) and an
[Ash Browser Test](#Ash-Browser-Tests), as end-to-end tests that exercise as
many layers as possible, is also recommended to ensure browser-side checks work
correctly, e.g. permissions, navigations, etc.

### ChromeOS App APIs that rely on real device features
In addition to the cases above, if your API uses services that are only
available on real devices e.g. the ML Services, Croshealthd, etc., you should
strongly consider writing Tast/Crosier tests to cover CUJs. The majority of the
testing should still be done as described in the other sections.
