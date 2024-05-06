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
   For an example see `components/pdf/renderer/pdf_ocr_helper.cc`.

## How to use Main Content Extraction
In the browser process call
`screen_ai:ScreenAIServiceRouterFactory:GetForBrowserContext:GetServiceStateAsync`
to trigger library download and service initialization and receive the result in
a callback.\
Once you know the service is ready, trigger connection to it in your process by
connecting to `screen_ai:mojom:Screen2xMainContentExtractor` interface.\
For an example see `chrome/renderer/accessibility/ax_tree_distiller.cc`.

## Bugs Component:
  Chromium > UI > Accessibility > MachineIntelligence (component id: 1457124)
