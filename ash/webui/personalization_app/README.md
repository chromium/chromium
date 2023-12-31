# ChromeOS Personalization

## User Types and Profiles

ChromeOS Personalization features interact differently with different user
types. It is important for feature development to consider how the different
user types will be affected.

see: [`//components/user_manager/user_type.h`](../../../components/user_manager/user_type.h)

For a more in depth explanation, see:

[Profiles, Sessions, Users, and more for ChromeOS Personalization](http://go/chromeos-personalization-user-types)

## Tests

### Background

Personalization App takes a layered approach to testing. There are C++ unit
tests, javascript component browser tests, and javascript full-app browsertests.

* mojom handler unit tests
  * `//chrome/browser/ash/system_web_apps/apps/personalization_app/*unittest.cc`
  * `unit_tests --gtest_filter=*PersonalizationApp*`
  * primarily to test behavior of mojom handlers
  * heavily mocked out ash environment
    * fake user manager
    * fake wallpaper\_controller
    * etc
* component browser tests
  * `personalization_app_component_browsertest.cc`
  * `browser_tests --gtest_filter=*PersonalizationAppComponent*`
  * loads test cases from `//chrome/test/data/webui/chromeos/personalization_app/*`
  * Opens an empty browser window, loads javascript necessary to render a
    single Polymer element, and executes javascript tests against that component
  * All mojom calls are faked in javascript
    * any mojom call that reaches
    `personalization_app_mojom_banned_mocha_test_base.h`
    will immediately fail the test
* controller browser tests
  * `personalization_app_controller_browsertest.cc`
  * `browser_tests --gtest_filter=*PersonalizationAppController*`
  * no UI elements, javascript testing of controller functions, reducers, logic
  * All mojom calls are faked in javascript the same way as component browser
  tests
* app browser tests
  * `personalization_app_test.ts`
  * `browser_tests --gtest_filter=*PersonalizationAppBrowserTest`
  * Uses fixture `personalization_app_mocha_test_base.h`
    * wallpaper mocked out at network layer by mocking out wallpaper fetchers
    via `TestWallpaperFetcherDelegate`
    * uses a real theme provider
    * all others mock out mojom layer via fake mojom providers
    `FakePersonalizationApp{Ambient,KeyboardBacklight,User}Provider`
* System Web App integration tests
  * `personalization_app_integration_browsertest.cc`
  * `browser_tests --gtest_filter=*PersonalizationAppIntegration*`
  * Tests that the app install, launches without error
  * Also tests special tricky system UI support for full screen transparency for
  wallpaper preview because they cannot be tested in javascript
    * includes a pixel test for fullscreen wallpaper preview
    * see below [Running browser pixel tests](#running-browser-pixel-tests) and
    `//ash/test/pixel/README.md` for more information

#### Running browser pixel tests

##### Locally

```
browser_tests
--gtest_filter=*PersonalizationAppIntegrationPixel*
--skia-gold-local-png-write-directory=/tmp/skia_gold/
--enable-pixel-output-in-tests
--browser-ui-tests-verify-pixels
```

Inspect the output pngs generated in `/tmp/skia_gold/*` to make sure that the
test is setting up the correct UI state.

##### CQ

In CQ these tests do not actually verify pixel output as part of the mainline
`browser_tests` step in `linux-chromeos-rel`. However, they still go through
the setup to make sure there are no crashes while preparing the UI. Full pixel
verification will run as part of `pixel_experimental_browser_tests` which passes
the necessary additional argument `--browser-ui-tests-verify-pixels`.

### Where should I write my test?

* complex behavior that involves multiple parts of the application and mojom
handlers
  * app browser tests
* a single javascript component
  * component browser tests
* javascript logic and state management
  * controller browser tests
* mojom handling logic
  * mojom handler unit tests

### Debugging tests
* [Debugging BrowserTest failures](https://g3doc.corp.google.com/chrome/chromeos/system_services_team/dev_instructions/g3doc/debugging.md#debugging-browsertest-failures).
* The [browser test doc](https://www.chromium.org/developers/testing/browser-tests/#debugging)
has some useful information.
* Inject `debugger;` as a breakpoint.
* Run a specific test/test suite: `test("test name", () => ...) => test.only("test name"...)`.
* Debug flaky tests: Pass flags `--gtest_repeat=1000 --gtest_break_on_failure`.

## Environment Setup
### VSCode

- Follow [vscode setup](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/vscode.md).
  - (Optional) Set up [code-server](http://go/vscode/remote_development_via_web) for remote development.
- Create `tsconfig.json` using [helper script](https://chromium.googlesource.com/chromium/src/+/HEAD/ash/webui/personalization_app/tools/gen_tsconfig.py).
  Please follow the help doc in the header of the helper script.
- Edit `${PATH_TO_CHROMIUM}/src/.git/info/exclude` and add these lines
  ```
  /ash/webui/personalization_app/resources/tsconfig.json
  /chrome/test/data/webui/chromeos/personalization_app/tsconfig.json
  ```
