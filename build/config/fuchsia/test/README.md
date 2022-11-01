## Manifest Fragments

This directory contains the manifest fragments that are required for running
Fuchsia tests hermetically. Tests start from `minimum.shard.test-cml` and add
additional capabilities as necessary by providing the
`additional_manifest_fragments` argument. Some fragments are explained in detail
below:

### General Purpose Fragments

#### archivist.shard.test-cml
Runs an `archivist-without-attribution` with custom protocol routing for tests
that want to intercept events written to a `LogSink` by a component.

#### chromium_test_facet.shard.test-cml
Runs CFv2 tests in the "chromium" test realm. This is generally required for all
Chromium tests that must interact with true system services.

#### fonts.shard.test-cml
For tests that test fonts by providing `fuchsia.fonts.Provider`. This shard
runs an isolated font provider, but serves the fonts present on the system.

#### test_fonts.shard.test-cml
For tests that use the fonts in `//third_party/test_fonts` by way of
`//skia:test_fonts_cfv2`.

#### mark_vmo_executable.shard.test-cml
Required by tests that execute JavaScript. Should only be required in a small
number of tests.

#### minimum.shard.test-cml
Capabilities required by anything that uses `//base/test`, used as the base
fragment for all test suites.

`config-data` is included in the features list so that the platform can offer
ICU timezone data to these tests when they are being run.  A more general
approach is discussed in https://fxbug.dev/85845.

#### logger.shard.test-cml
For tests that test logging functionality by providing `fuchsia.logger.Log`.

#### test_ui_stack.shard.test-cml
For tests that need an isolated UI subsystem, that supports the Flatland
API set.  This allows tests to e.g. run with view-focus unaffected by any
other tests running concurrently on the device, as well as providing test-only
functionality such as input-injection support.

#### gfx_test_ui_stack.shard.test-cml
For tests that need an isolated display subsystem supporting the legacy
Scenic/GFX APIs.

### WebEngine Fragments
The following fragments are specific to WebEngine functionality as documented
documentation at
https://fuchsia.dev/reference/fidl/fuchsia.web#CreateContextParams and
https://fuchsia.dev/reference/fidl/fuchsia.web#ContextFeatureFlags.
Any test-specific exceptions are documented for each file.

#### audio_output.shard.test-cml
Required by tests that need to enable audio output.

#### platform_video_codecs.shard.test-cml
Required by tests that need accelerated (e.g., hardware) video codecs. A private
(semi-isolated) instance of codec_factory is run for tests using this shard in
support of running on system images that don't run it.

#### network.shard.test-cml
For tests that need access to network services, including those that access a
local HTTP server.

#### network.shard.test-cml
Corresponds to the `NETWORK` flag. Required for enabling network access. Note
that access to the root SSL certificates is not needed if ContextProvider is
used to launch the `Context`. The `fuchsia.device.NameProvider` dependency comes
from fdio.

#### present_view.shard.test-cml
Services that are needed to render web content in a Scenic view and present it.
Most services are required per the FIDL documentation.

#### web_instance.shard.test-cml
Contains services that need to be present when creating a `fuchsia.web.Context`.
Note that the `fuchsia.scheduler.ProfileProvider` service is only used in tests
that encounter memory pressure code.

#### web_instance_host.shard.test-cml
Contains services that need to be present to use `WebInstanceHost`.
