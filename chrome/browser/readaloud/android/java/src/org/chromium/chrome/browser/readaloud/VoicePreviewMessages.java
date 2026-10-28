// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import org.chromium.base.LocaleUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Map;

/**
 * Bundled translations of IDS_READALOUD_VOICE_PREVIEW_MESSAGE for the languages supported by native
 * Read Aloud voices.
 *
 * <p>Native voice previews use these translations instead of Android resources because, in Android
 * App Bundle builds, translated resources live in per-language splits that are only installed for
 * the user's system/app languages.
 *
 * <p>Manually copied from chrome/browser/ui/android/strings/android_chrome_strings.grd (English)
 * and chrome/browser/ui/android/strings/translations/android_chrome_strings_*.xtb. Update this file
 * when IDS_READALOUD_VOICE_PREVIEW_MESSAGE or its translations change.
 */
@NullMarked
final class VoicePreviewMessages {
    private static final Map<String, String> TRANSLATIONS =
            Map.ofEntries(
                    Map.entry(
                            "ar",
                            "إليك الأصوات التي يمكنك الاختيار منها لقراءة صفحات الويب. إذا أعجبك"
                                + " هذا الصوت وأردت أن أستمر في استخدامه، يُرجى النقر على اسمه."),
                    Map.entry(
                            "bn",
                            "আপনার ওয়েব পৃষ্ঠা জোরে পড়ে শোনানোর জন্য এইসব ভয়েস বেছে নিতে পারবেন।"
                                + " এই ভয়েস যদি পছন্দ করেন এবং চান যে এটি ব্যবহার করা চালিয়ে যাই,"
                                + " শুধুমাত্র ভয়েসের নামের উপরে ট্যাপ করুন।"),
                    Map.entry(
                            "de",
                            "Hier kannst du die Stimme auswählen, mit der deine Webseiten"
                                + " vorgelesen werden. Wenn dir diese Stimme gefällt und du"
                                + " möchtest, dass ich sie weiterhin verwende, tippe einfach auf"
                                + " den Namen der Stimme."),
                    Map.entry(
                            "el",
                            "Αυτές είναι οι φωνές που μπορείτε να επιλέξετε για την ανάγνωση των"
                                + " ιστοσελίδων σας. Εάν σας αρέσει αυτή η φωνή και θέλετε να"
                                + " συνεχίσω να τη χρησιμοποιώ, απλώς πατήστε το όνομα της φωνής."),
                    Map.entry(
                            "en",
                            "Here are the voices you can pick for reading out your web pages. If"
                                    + " you like this voice and want me to keep using it just tap"
                                    + " the voice name."),
                    Map.entry(
                            "es",
                            "Estas son las voces que puedes elegir para leer tus páginas web en voz"
                                    + " alta. Si te gusta esta y quieres que la siga usando, solo"
                                    + " tienes que tocar su nombre."),
                    Map.entry(
                            "fr",
                            "Voici les voix que je peux utiliser pour lire le contenu de vos pages"
                                    + " Web. Si cette voix vous plaît et que vous voulez que je"
                                    + " continue de l'utiliser, appuyez sur son nom."),
                    Map.entry(
                            "he",
                            "אלה הקולות שאפשר לבחור להקראה של דפי האינטרנט. אם הקול הזה מוצא חן"
                                + " בעיניך וברצונך שאמשיך להשתמש בו, פשוט צריך ללחוץ על שם הקול."),
                    Map.entry(
                            "hi",
                            "वेब पेजों के कॉन्टेंट को अपनी पसंदीदा आवाज़ में सुनने के लिए, इन आवाज़ों"
                                + " को आज़माया जा सकता है. अगर आपको यह आवाज़ पसंद है और आपको मुझसे"
                                + " इस आवाज़ में कॉन्टेंट पढ़ाना है, तो आवाज़ के नाम पर टैप करें."),
                    Map.entry(
                            "id",
                            "Berikut suara yang dapat dipilih untuk membacakan halaman web Anda."
                                + " Jika Anda suka suara ini dan ingin terus menggunakannya, cukup"
                                + " ketuk nama suara tersebut."),
                    Map.entry(
                            "it",
                            "Ecco le voci che puoi scegliere per la lettura ad alta voce delle"
                                    + " pagine web. Se ti piace questa voce e vuoi che continui a"
                                    + " utilizzarla, tocca il suo nome."),
                    Map.entry(
                            "ja",
                            "ウェブページの読み上げに選択できる音声はこちらです。この音声を気に入って引き続き使用する場合は、音声の名前をタップしてください。"),
                    Map.entry(
                            "ml",
                            "നിങ്ങളുടെ വെബ് പേജുകൾ വായിക്കാൻ തിരഞ്ഞെടുക്കാവുന്ന ശബ്ദങ്ങൾ ഇവയാണ്."
                                    + " നിങ്ങൾക്ക് ഈ ശബ്ദം ഇഷ്ടപ്പെടുകയും തുടർന്നും ഞാനിത്"
                                    + " ഉപയോഗിക്കണമെന്ന് ആഗ്രഹിക്കുകയും ചെയ്യുന്നുണ്ടെങ്കിൽ,"
                                    + " ശബ്ദത്തിന്റെ പേരിൽ ടാപ്പ് ചെയ്യുക."),
                    Map.entry(
                            "mr",
                            "तुमची वेब पेज मोठ्याने वाचण्यासाठी तुम्ही निवडू शकता असे आवाज येथे"
                                    + " आहेत. तुम्हाला हा आवाज आवडल्यास आणि मी तो वापरावा असे वाटत"
                                    + " असल्यास, फक्त त्या आवाजाच्या नावावर क्लिक करा."),
                    Map.entry(
                            "pl",
                            "Oto głosy, którymi mogę czytać dla Ciebie strony internetowe. Jeśli"
                                + " podoba Ci się ten głos i\u00a0chcesz go dalej używać, kliknij"
                                + " jego nazwę."),
                    // Portuguese uses the pt-BR translation, matching the region-less locale used
                    // for the resource lookup.
                    Map.entry(
                            "pt",
                            "Estas são as vozes que você pode escolher para ler suas páginas da Web"
                                + " em voz alta. Se quiser que eu continue usando esta voz, toque"
                                + " no nome dela."),
                    Map.entry(
                            "ro",
                            "Iată vocile pe care le poți alege pentru a-ți citi paginile web. Dacă"
                                    + " îți place această voce și vrei să o folosesc în continuare,"
                                    + " atinge numele vocii."),
                    Map.entry(
                            "ru",
                            "Здесь представлены голоса для озвучивания текста на веб-страницах."
                                    + " Нажмите на название понравившегося голоса, и я буду"
                                    + " использовать именно его."),
                    Map.entry(
                            "ta",
                            "இணையப் பக்கங்களை உங்களுக்காக வாசிக்க இந்தக் குரல்களில் இருந்து நீங்கள்"
                                + " தேர்ந்தெடுக்கலாம். இந்தக் குரல் பிடித்திருந்தால் அதிலேயே நான்"
                                + " தொடர வேண்டும் என்றால் குரலின் பெயர் மீது தட்டினால் போதும்."),
                    Map.entry(
                            "te",
                            "మీ వెబ్ పేజీలోని కంటెంట్\u200cను చదవడానికి మీరు ఎంచుకోగల"
                                    + " వాయిస్\u200cలు ఇక్కడ ఉన్నాయి. మీకు వాయిస్ నచ్చితే, దాన్ని"
                                    + " మీరు ఉపయోగించడం కొనసాగించాలనుకుంటే, వాయిస్ పేరుపై ట్యాప్"
                                    + " చేయండి."),
                    Map.entry(
                            "th",
                            "นี่คือเสียงที่คุณเลือกใช้ในการอ่านหน้าเว็บได้"
                                    + " หากชอบเสียงนี้และต้องการใช้ต่อ ก็เพียงแตะที่ชื่อของเสียง"),
                    Map.entry(
                            "tr",
                            "Web sayfalarınızı sesli okuması için seçebileceğiniz sesleri burada"
                                    + " bulabilirsiniz. Bu sesi beğendiyseniz ve kullanmaya devam"
                                    + " etmemi istiyorsanız sesin adına dokunmanız yeterli."),
                    Map.entry(
                            "uk",
                            "Тут можна вибрати голос для озвучення веб-сторінок. Якщо вам"
                                    + " подобається цей голос і ви хотіли б чути його надалі,"
                                    + " натисніть його назву."),
                    Map.entry(
                            "vi",
                            "Đây là những giọng nói mà bạn có thể chọn để tôi đọc to nội dung các"
                                + " trang web. Nếu bạn thích giọng nói này và muốn tôi dùng tiếp,"
                                + " chỉ cần nhấn vào tên giọng nói."),
                    Map.entry("zh", "您可以在这里选择系统朗读网页内容时要使用的声音。如果您喜欢这个声音，并且希望我继续使用它，只要点按声音名称就可以了。"));

    private VoicePreviewMessages() {}

    /**
     * Returns the bundled voice preview message for {@code language}, or null if there is none.
     *
     * @param language BCP-47 language tag of the voice (e.g. "es" or "es-US"), or null.
     */
    static @Nullable String get(@Nullable String language) {
        if (language == null) {
            return null;
        }
        String baseLanguage =
                LocaleUtils.getUpdatedLanguageForChromium(LocaleUtils.toBaseLanguage(language));
        return TRANSLATIONS.get(baseLanguage);
    }
}
