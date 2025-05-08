# Chrome Screen AI Library

## Purpose
ScreenAI service provides accessibility helpers, is downloaded and initialized
on demand, and stays on disk for 30 days after the last use.\
The service is created per profile and will stay alive as long as the profile
lives.\
See `services/screen_ai/README.md` for more.

## How to Use for OCR
Depending on your use case restrictions, choose one of the following
approaches.
1. If you are adding a new client for OCR, add a new enum value to
   `screen_ai::mojom::OcrClientType`, otherwise choose an appropriate one for it
   in the next steps.
1. Join `chrome-ocr-clients@` group to get notifications on major updates.
1. Using `OpticalCharacterRecognizer:CreateWithStatusCallback`, create an OCR
   object, and wait until the callback is called. This will trigger download
   and startup of the service (if needed) and reports the result.\
   Once the callback is called with `true` value, use
   `OpticalCharacterRecognizer:PerformOCR`.\
   Creation of the object can only be done in the UI thread.
1. If you cannot use the callback, create the object using
   `OpticalCharacterRecognizer:Create` and keep calling
   `OpticalCharacterRecognizer:is_ready` until it tells you that the service
   is ready.\
   Then use `OpticalCharacterRecognizer:PerformOCR` as above.\
   Creation of the object can only be done in the UI thread.
1. If neither of the above work, in the browser process call
   `screen_ai:ScreenAIServiceRouterFactory:GetForBrowserContext:GetServiceStateAsync`
   to trigger library download and service initialization and receive the result
   in a callback.\
   Once you know the service is ready, trigger connection to it in your process
   by connecting to `screen_ai:mojom:ScreenAIAnnotator` interface.\
   Before calling any of the `PerformOCR` functions, call `SetClientType` once
   to set the client type.\
   For an example see `components/pdf/renderer/pdf_ocr_helper.cc`.

## How to use Main Content Extraction
If you are adding a new client for MCE, add a new enum value to
`screen_ai::mojom::MceClientType`.
In the browser process call
`screen_ai:ScreenAIServiceRouterFactory:GetForBrowserContext:GetServiceStateAsync`
to trigger library download and service initialization and receive the result in
a callback.\
Once you know the service is ready, trigger connection to it in your process by
connecting to `screen_ai:mojom:Screen2xMainContentExtractor` interface.\
Call `SetClientType` once to set the client type.\
For an example see `chrome/renderer/accessibility/ax_tree_distiller.cc`.

## Cautions and Best Practices
1. OCR downsamples the images if they are larger than a certain threshold, which
   you can get through `GetMaxImageDimension` function from version 138. Sending
   images with higher resolution will not increase the recognition quality and
   only increases allocated memory and adds extra processing time. If you are
   resizing the image that you sent to OCR for any other reasons, consider this
   threshold.
1. ScreenAI service has a large memory footprint and should be purged from
   memory when it's not needed. To do so, it monitors last used time and if it
   is not used for sometime (currently 3 seconds), it shuts down and restarts
   the next time it is needed.
1. Have support code for possible disconnecting from the service and
   reconnecting if needed. This can happen due to a service crash or shutdown on
   being idle.
1. If the service crashes, it suspends itself for sometime (increasing on
   subsequent crashes). Make sure your usecase is consistent with it. You can
   get the actual delay for the nth crash through
   `SuggestedWaitTimeBeforeReAttempt` function.
1. If you have a batch job, send one request at a time to the service to avoid
   bloating the queue for tasks. Mechanisms may be added soon to kill the
   process if it allocates too much memory. Also consider adding pauses after
   every few requests so that system resources would not be allocated a lot for
   a long continuous time.

## Bugs Component
  Chromium > UI > Accessibility > MachineIntelligence (component id: 1457124)
