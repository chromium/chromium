An IME Mojo service provides core IME functionality.

IME on ChromeOS consists of three parts:
- The IME running in an extension to provide a soft keyboard
- The sandboxed, core IME service running in ChromeOS (responsible for
  processing input and turning it into characters/symbols)
- The IME framework running in the Chrome browser process, that brokers
  between the IME extension and the IME service. It also provides additional
  functionality to the IME service (for downloading IME data as needed).

The service provides basic rule-based IMEs, and is able to support
advanced IME features by loading a shared library.

Work in progress, design doc: go/cros-ime-decoders-mojo
