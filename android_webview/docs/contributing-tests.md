# Contributing to WebView Tests

[TOC]

## Instrumentation tests
These are on-device integration tests for android\_webview with rest of the
chromium code (covering both Java and native code). A large percentage of
Android applications use WebView, and could trigger WebView specific code paths
in the codebase, therefore it is important to have solid integration tests.

#### Where to add tests?
The tests are located in the [javatests directory](/android_webview/javatests/src/org/chromium/android_webview/test/).
They are roughly organized by the
[Aw classes](/android_webview/java/src/org/chromium/android_webview/) (some
tests are organized by overall functionality instead). Put new tests into an
existing test class or create a new one if a suitable one isn't available.

#### How to write intrumentation tests?
See the [instrumentation test doc](/docs/testing/android_instrumentation_tests.md).

#### How do tests interact with WebView?
Tests are written as JUnit4 tests. [AwActivityTestRule] is used to create and
obtain references to WebView objects such as [AwContents](internally it launches
the [WebView instrumentation shell](/android_webview/test/shell/src/org/chromium/android_webview/shell/)
application to hold [AwTestContainerViews](/android_webview/test/shell/src/org/chromium/android_webview/test/AwTestContainerView.java)
which in turn contains [AwContents]). [AwContents] will allow the test to
trigger code paths such as loading urls, going forwards/backwards, etc... .
[AwActivityTestRule] has some helper methods to call the [AwContents] methods,
for example to ensure that they are called on the UI thread. Some AW
components, such as [AwCookieManager](/android_webview/java/src/org/chromium/android_webview/AwCookieManager.java),
can be directly created in tests.

#### How do tests inject html/css/js content?
Tests can use the load\* methods in [AwActivityTestRule] to inject snippets
of content. This will however bypass the network layer. To have end-to-end
testing, use [EmbeddedTestServer](/net/test/android/javatests/src/org/chromium/net/test/EmbeddedTestServer.java),
which will allow simple loading of files from the [data directory](/android_webview/test/data/).
For loading data from arbitrary URIs and more advanced control, the
[TestWebServer](/net/test/android/javatests/src/org/chromium/net/test/util/TestWebServer.java)
can be used. Note that, when simulating input or user actions on web content,
the content should include some visiible text because input is typically
ignored until something meaningful is painted.

## Java unittest (JUnit)
These are off-device tests using robolectric that only exercise android\_webview
Java code.

#### Where to add tests?
The tests are located in the [junit directory](/android_webview/junit/src/org/chromium/android_webview/robolectric/).

#### How to write junit tests?
See the [JUnit doc](/docs/testing/android_robolectric_tests.md).

## Native unittests
These are on-device gtests that only exercise android\_webview native code.

#### Where to add tests?
The tests are located alongside respective source code files under
[android\_webview directory](/android_webview/).

#### How to write gtests?
See the [GTest doc](/docs/testing/android_gtests.md).

## How to run tests?
Running tests is covered in [WebView Test Instructions](/android_webview/docs/test-instructions.md).

[AwActivityTestRule]:
/android_webview/javatests/src/org/chromium/android_webview/test/AwActivityTestRule.java
[AwContents]:
/android_webview/java/src/org/chromium/android_webview/AwContents.java
