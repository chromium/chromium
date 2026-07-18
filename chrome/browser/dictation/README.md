# Dictation

(Googlers only for now): https://docs.google.com/document/d/1Zri5oR3P3D5LOq9Gjs4AeHFrMMOX7bTtctxT6KrGeSQ/edit?usp=sharing

## Concepts

### Session

The top-level lifecycle of a Dictation interaction. A session begins when the
user invokes the feature (e.g., via a context menu) and ends when the UI is
dismissed or the task is completed. A session coordinates one or more
speech recognition streams.

### Stream

A single period of audio capture and speech recognition.

* A stream is always bound to exactly one Target.
* Only one stream can be "live" (actively capturing microphone input) at a time.
* Streams can overlap: a new stream may start while a previous one is still
  **Finalizing** (processing the remaining audio data from the backend).

### Attached Stream

The current stream in a session that is primary to the user's interaction. The
"Attached" status determines which stream's state (e.g., volume levels,
processing status) is reflected in the session's UI.

### Finalizing
A state where a stream has stopped capturing audio (the microphone is off) but
is still awaiting final data from the backend. This might include:

* Receiving the final, corrected transcript.
* Applying recognized commands or transformations (e.g., "make this more
  formal").

Multiple streams can be in this state simultaneously as they flush their
remaining backend data.

### Target

An abstraction representing an editable field (e.g., an HTML `<input>`,
`<textarea>`, `EditContext`, or `contenteditable` element).

* It is responsible for observing the underlying DOM element. If the element is
  removed or disabled, the target notifies its provider.
* It is responsible for orchestrating the insertion of text into the DOM.

### Commit

The final operation where the stable, transformed transcription is inserted into
the target. A commit only occurs once the text is finalized (i.e., after all
transcriptions and transformations are complete). For MVP, this is an atomic
operation at the end of a stream.

## Implementation

Speech recognition is provided by a cloud hosted service. The implementation in
Chrome connects to this service via a component extension we call the
"connector" or "connector extension". The core dictation logic in the browser
process communicates with the connector extension via a private extension API.

The extension itself is built outside of the chromium source tree and provided
via the component updater.

## Implementation Key Files

### //chrome/browser/dictation

Core dictation logic

* dictation_keyed_service.h - profile-global object,
  always available if the feature is enabled to serve as the entrypoint and
  central coordinator class.
* session_controller.h - Implements a dictation
  session which coordinates the speech recognition stream and UI.
* session_ui_impl.h - Implements the UI for a
  dictation session. This object controls the browser UI views and is the
  handler logic for all UI triggered actions.
* listener_stream_provider.h - Implements the
  speech recognition stream. Starts a connection to the recognition service via
  the connector extension using the private dictation API.
* dictation_multiplexer.h - Associates each stream
  provider with an id and receives messages from the connector extension via
  the private dictation API. This object is responsible for routing the
  messages to the appropriate stream provider object.
* dictation_context_fetcher.h - Responsible for collecting any relevant context
  from the page being dictated into. Used by a stream provider to send page
  context to the connector extension.

### chrome/browser/extensions/api/dictation_private/

C++ implementation of the private extension API used to communicate between
core dictation impl in the browser and the connector extension.

* dictation_private_api.h - C++ implementation of the privateDictation API
  called by the extension

### chrome/common/extensions/api/

The IDL defining the API exposed to the connector extension

* chrome/common/extensions/api/dictation_private.webidl

(See https://crrev.com/c/7871142 for all the files touched relevant to the
extension API)

### chrome/browser/ui/views/dictation/

Views UI components

* dictation_bubble_ui.h - The primary UI widget which implements a toast-like
  bubble UI in which the dictation session takes place.

### chrome/browser/renderer_context_menu/

* dictation_menu_observer.h - Implements the start dictation context menu item

* render_view_context_menu.cc - Where the context menu item is registered in
  the menu

### chrome/test/data/extensions/dictation/

See test extension [README.md](/chrome/test/data/extensions/dictation/README.md)

* background.js - Service worker script for the "test extension" used by
  dictation tests. Dictation tests put this test into "manual mode" and use it
  to communicate with the other side of the extension API.

## Logging

VT_LOG messages can be printed on the commandline by starting Chrome with:
`--vmodule="*dictation*=1,*session*=1,*stream*=1" --enable-logging=stderr`
