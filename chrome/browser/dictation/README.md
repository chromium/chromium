# Dictation

(Googlers only for now): https://docs.google.com/document/d/1Zri5oR3P3D5LOq9Gjs4AeHFrMMOX7bTtctxT6KrGeSQ/edit?usp=sharing

## Concepts

### Session

The top-level lifecycle of a Dictation interaction. A session begins when the
user invokes the feature (e.g., via a context menu) and ends when the UI is
dismissed or the task is completed. A session coordinates one or more
transcription streams.

### Stream

A single period of audio capture and transcription.

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
