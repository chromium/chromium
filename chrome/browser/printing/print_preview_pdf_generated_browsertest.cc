// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/md5.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/test/browser_test_utils.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/filename_util.h"
#include "pdf/pdf.h"
#include "printing/pdf_render_settings.h"
#include "printing/units.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include <fcntl.h>
#include <io.h>
#endif

using content::WebContents;
using content::WebContentsObserver;

namespace printing {

// Number of color channels in a BGRA bitmap.
const int kColorChannels = 4;
const int kDpi = 300;

// Every state is used when the document is a non-PDF source. When the source is
// a PDF, kWaitingToSendSaveAsPDF, kWaitingToSendPageNumbers, and
// kWaitingForFinalMessage are the only states used.
enum State {
  // Waiting for the first message so the program can select Save as PDF
  kWaitingToSendSaveAsPdf = 0,
  // Waiting for the second message so the test can set the layout
  kWaitingToSendLayoutSettings = 1,
  // Waiting for the third message so the test can set the page numbers
  kWaitingToSendPageNumbers = 2,
  // Waiting for the forth message so the test can set the headers checkbox
  kWaitingToSendHeadersAndFooters = 3,
  // Waiting for the fifth message so the test can set the background checkbox
  kWaitingToSendBackgroundColorsAndImages = 4,
  // Waiting for the sixth message so the test can set the margins combobox
  kWaitingToSendMargins = 5,
  // Waiting for the final message so the program can save to PDF.
  kWaitingForFinalMessage = 6,
};

// Settings for print preview. It reflects the current options provided by
// print preview. If more options are added, more states should be added and
// there should be more settings added to this struct.
struct PrintPreviewSettings {
  PrintPreviewSettings(bool is_portrait,
                       const std::string& page_numbers,
                       bool headers_and_footers,
                       bool background_colors_and_images,
                       MarginType margins,
                       bool source_is_pdf)
      : is_portrait(is_portrait),
        page_numbers(page_numbers),
        headers_and_footers(headers_and_footers),
        background_colors_and_images(background_colors_and_images),
        margins(margins),
        source_is_pdf(source_is_pdf) {}

  bool is_portrait;
  std::string page_numbers;
  bool headers_and_footers;
  bool background_colors_and_images;
  MarginType margins;
  bool source_is_pdf;
};

// Observes the print preview webpage. Once it observes the PreviewPageCount
// message, will send a sequence of commands to the print preview dialog and
// change the settings of the preview dialog.
class PrintPreviewObserver : public WebContentsObserver {
 public:
  PrintPreviewObserver(Browser* browser,
                       WebContents* dialog,
                       const base::FilePath& pdf_file_save_path)
      : WebContentsObserver(dialog),
        browser_(browser),
        state_(kWaitingToSendSaveAsPdf),
        failed_setting_("None"),
        pdf_file_save_path_(pdf_file_save_path) {}

  ~PrintPreviewObserver() override {}

  // Sets closure for the observer so that it can end the loop.
  void set_quit_closure(base::OnceClosure closure) {
    quit_closure_ = std::move(closure);
  }

  // Actually stops the message loop so that the test can proceed.
  void EndLoop() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(quit_closure_));
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(PrintPreviewObserver, message)
      IPC_MESSAGE_HANDLER(PrintHostMsg_DidStartPreview, OnDidStartPreview)
    IPC_END_MESSAGE_MAP()
    return false;
  }

  // Gets the web contents for the print preview dialog so that the UI and
  // other elements can be accessed.
  WebContents* GetDialog() {
    WebContents* tab = browser_->tab_strip_model()->GetActiveWebContents();
    PrintPreviewDialogController* dialog_controller =
        PrintPreviewDialogController::GetInstance();
    return dialog_controller->GetPrintPreviewForContents(tab);
  }

  // Gets the PrintPreviewUI so that certain elements can be accessed.
  PrintPreviewUI* GetUI() {
    return static_cast<PrintPreviewUI*>(
        GetDialog()->GetWebUI()->GetController());
  }

  // Calls native_layer.onManipulateSettingsForTest() and sends a dictionary
  // value containing the type of setting and the value to set that settings
  // to.
  void ManipulatePreviewSettings() {
    base::DictionaryValue script_argument;

    if (state_ == kWaitingToSendSaveAsPdf) {
      script_argument.SetBoolean("selectSaveAsPdfDestination", true);
      state_ = settings_->source_is_pdf ?
               kWaitingToSendPageNumbers : kWaitingToSendLayoutSettings;
      failed_setting_ = "Save as PDF";
    } else if (state_ == kWaitingToSendLayoutSettings) {
      script_argument.SetBoolean("layoutSettings.portrait",
                                 settings_->is_portrait);
      state_ = kWaitingToSendPageNumbers;
      failed_setting_ = "Layout Settings";
    } else if (state_ == kWaitingToSendPageNumbers) {
      script_argument.SetString("pageRange", settings_->page_numbers);
      state_ = settings_->source_is_pdf ?
               kWaitingForFinalMessage : kWaitingToSendHeadersAndFooters;
      failed_setting_ = "Page Range";
    } else if (state_ == kWaitingToSendHeadersAndFooters) {
      script_argument.SetBoolean("headersAndFooters",
                                 settings_->headers_and_footers);
      state_ = kWaitingToSendBackgroundColorsAndImages;
      failed_setting_ = "Headers and Footers";
    } else if (state_ == kWaitingToSendBackgroundColorsAndImages) {
      script_argument.SetBoolean("backgroundColorsAndImages",
                                 settings_->background_colors_and_images);
      state_ = kWaitingToSendMargins;
      failed_setting_ = "Background Colors and Images";
    } else if (state_ == kWaitingToSendMargins) {
      script_argument.SetInteger("margins", settings_->margins);
      state_ = kWaitingForFinalMessage;
      failed_setting_ = "Margins";
    } else if (state_ == kWaitingForFinalMessage) {
      // Called by |GetUI()->handler_|, it is a callback function that call
      // |EndLoop| when an attempt to save the PDF has been made.
      GetUI()->SetPdfSavedClosureForTesting(base::BindOnce(
          &PrintPreviewObserver::EndLoop, base::Unretained(this)));
      ASSERT_FALSE(pdf_file_save_path_.empty());
      GetUI()->SetSelectedFileForTesting(pdf_file_save_path_);
      return;
    }

    ASSERT_FALSE(script_argument.empty());
    GetUI()->SendManipulateSettingsForTest(script_argument);
  }

  // Saves the print preview settings to be sent to the print preview dialog.
  void SetPrintPreviewSettings(const PrintPreviewSettings& settings) {
    settings_ = std::make_unique<PrintPreviewSettings>(settings);
  }

  // Returns the setting that could not be set in the preview dialog.
  const std::string& GetFailedSetting() const {
    return failed_setting_;
  }

 private:
  // Listens for messages from the print preview dialog. Specifically, it
  // listens for 'UILoadedForTest' and 'UIFailedLoadingForTest.'
  class UIDoneLoadingMessageHandler : public content::WebUIMessageHandler {
   public:
    explicit UIDoneLoadingMessageHandler(PrintPreviewObserver* observer)
        : observer_(observer) {}

    ~UIDoneLoadingMessageHandler() override {}

    // When a setting has been set succesfully, this is called and the observer
    // is told to send the next setting to be set.
    void HandleDone(const base::ListValue* /* args */) {
      observer_->ManipulatePreviewSettings();
    }

    // Ends the test because a setting was not set successfully. Called when
    // this class hears 'UIFailedLoadingForTest.'
    void HandleFailure(const base::ListValue* /* args */) {
      FAIL() << "Failed to set: " << observer_->GetFailedSetting();
    }

    // Allows this class to listen for the 'UILoadedForTest' and
    // 'UIFailedLoadingForTest' messages. These messages are sent by the print
    // preview dialog. 'UILoadedForTest' is sent when a setting has been
    // successfully set and its effects have been finalized.
    // 'UIFailedLoadingForTest' is sent when the setting could not be set. This
    // causes the browser test to fail.
    void RegisterMessages() override {
      web_ui()->RegisterMessageCallback(
          "UILoadedForTest",
          base::BindRepeating(&UIDoneLoadingMessageHandler::HandleDone,
                              base::Unretained(this)));

      web_ui()->RegisterMessageCallback(
          "UIFailedLoadingForTest",
          base::BindRepeating(&UIDoneLoadingMessageHandler::HandleFailure,
                              base::Unretained(this)));
    }

   private:
    PrintPreviewObserver* const observer_;

    DISALLOW_COPY_AND_ASSIGN(UIDoneLoadingMessageHandler);
  };

  // Called when the observer gets the IPC message with the preview document's
  // properties.
  void OnDidStartPreview(const PrintHostMsg_DidStartPreview_Params& params,
                         const PrintHostMsg_PreviewIds& ids) {
    WebContents* web_contents = GetDialog();
    ASSERT_TRUE(web_contents);
    Observe(web_contents);

    PrintPreviewUI* ui = GetUI();
    ASSERT_TRUE(ui);
    ASSERT_TRUE(ui->web_ui());

    ui->web_ui()->AddMessageHandler(
        std::make_unique<UIDoneLoadingMessageHandler>(this));
    ui->SendEnableManipulateSettingsForTest();
  }

  void DidCloneToNewWebContents(WebContents* old_web_contents,
                                WebContents* new_web_contents) override {
    Observe(new_web_contents);
  }

  Browser* browser_;
  base::OnceClosure quit_closure_;
  std::unique_ptr<PrintPreviewSettings> settings_;

  // State of the observer. The state indicates what message to send
  // next. The state advances whenever the message handler calls
  // ManipulatePreviewSettings() on the observer.
  State state_;
  std::string failed_setting_;
  const base::FilePath pdf_file_save_path_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewObserver);
};

class PrintPreviewPdfGeneratedBrowserTest : public InProcessBrowserTest {
 public:
  PrintPreviewPdfGeneratedBrowserTest() {}
  ~PrintPreviewPdfGeneratedBrowserTest() override {}

  // Navigates to the given web page, then initiates print preview and waits
  // for all the settings to be set, then save the preview to PDF.
  void NavigateAndPrint(const base::FilePath::StringType& file_name,
                          const PrintPreviewSettings& settings) {
    print_preview_observer_->SetPrintPreviewSettings(settings);
    base::FilePath path(file_name);
    GURL gurl = net::FilePathToFileURL(base::MakeAbsoluteFilePath(path));

    ui_test_utils::NavigateToURL(browser(), gurl);

    base::RunLoop loop;
    print_preview_observer_->set_quit_closure(loop.QuitClosure());
    chrome::Print(browser());
    loop.Run();

    // Need to check whether the save was successful. Ending the loop only
    // means the save was attempted.
    base::File pdf_file(
        pdf_file_save_path_, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(pdf_file.IsValid());
  }

  // Converts the PDF to a PNG file so that the layout test can do an image
  // diff on this image and a reference image.
  void PdfToPng() {
    int num_pages;
    double max_width_in_points = 0;
    std::vector<uint8_t> bitmap_data;
    double total_height_in_pixels = 0;
    std::string pdf_data;

    ASSERT_TRUE(base::ReadFileToString(pdf_file_save_path_, &pdf_data));

    auto pdf_span = base::as_bytes(base::make_span(pdf_data));
    ASSERT_TRUE(
        chrome_pdf::GetPDFDocInfo(pdf_span, &num_pages, &max_width_in_points));

    ASSERT_GT(num_pages, 0);
    double max_width_in_pixels =
        ConvertUnitDouble(max_width_in_points, kPointsPerInch, kDpi);

    for (int i = 0; i < num_pages; ++i) {
      double width_in_points, height_in_points;
      ASSERT_TRUE(chrome_pdf::GetPDFPageSizeByIndex(
          pdf_span, i, &width_in_points, &height_in_points));

      double width_in_pixels = ConvertUnitDouble(
          width_in_points, kPointsPerInch, kDpi);
      double height_in_pixels = ConvertUnitDouble(
          height_in_points, kPointsPerInch, kDpi);

      // The image will be rotated if |width_in_pixels| is greater than
      // |height_in_pixels|. This is because the page will be rotated to fit
      // within a piece of paper. Therefore, |width_in_pixels| and
      // |height_in_pixels| have to be swapped or else they won't reflect the
      // dimensions of the rotated page.
      if (width_in_pixels > height_in_pixels)
        std::swap(width_in_pixels, height_in_pixels);

      total_height_in_pixels += height_in_pixels;
      gfx::Rect rect(width_in_pixels, height_in_pixels);
      PdfRenderSettings settings(rect, gfx::Point(0, 0), gfx::Size(kDpi, kDpi),
                                 /*autorotate=*/false,
                                 /*use_color=*/true,
                                 PdfRenderSettings::Mode::NORMAL);

      int int_max = std::numeric_limits<int>::max();
      if (settings.area.width() > int_max / kColorChannels ||
          settings.area.height() >
              int_max / (kColorChannels * settings.area.width())) {
        FAIL() << "The dimensions of the image are too large."
               << "Decrease the DPI or the dimensions of the image.";
      }

      std::vector<uint8_t> page_bitmap_data(kColorChannels *
                                            settings.area.size().GetArea());

      ASSERT_TRUE(chrome_pdf::RenderPDFPageToBitmap(
          pdf_span, i, page_bitmap_data.data(), settings.area.size().width(),
          settings.area.size().height(), settings.dpi.width(),
          settings.dpi.height(), settings.autorotate, settings.use_color));
      FillPng(&page_bitmap_data, width_in_pixels, max_width_in_pixels,
              settings.area.size().height());
      bitmap_data.insert(bitmap_data.end(),
                         page_bitmap_data.begin(),
                         page_bitmap_data.end());
    }

    CreatePng(bitmap_data, max_width_in_pixels, total_height_in_pixels);
  }

  // Fills out a bitmap with whitespace so that the image will correctly fit
  // within a PNG that is wider than the bitmap itself.
  void FillPng(std::vector<uint8_t>* bitmap,
               int current_width,
               int desired_width,
               int height) {
    ASSERT_TRUE(bitmap);
    ASSERT_GT(height, 0);
    ASSERT_LE(current_width, desired_width);

    if (current_width == desired_width)
      return;

    int current_width_in_bytes = current_width * kColorChannels;
    int desired_width_in_bytes = desired_width * kColorChannels;

    // The color format is BGRA, so to set the color to white, every pixel is
    // set to 0xFFFFFFFF.
    const uint8_t kColorByte = 255;
    std::vector<uint8_t> filled_bitmap(
        desired_width * kColorChannels * height, kColorByte);
    auto filled_bitmap_it = filled_bitmap.begin();
    auto bitmap_it = bitmap->begin();

    for (int i = 0; i < height; ++i) {
      std::copy(
          bitmap_it, bitmap_it + current_width_in_bytes, filled_bitmap_it);
      std::advance(bitmap_it, current_width_in_bytes);
      std::advance(filled_bitmap_it, desired_width_in_bytes);
    }

    bitmap->assign(filled_bitmap.begin(), filled_bitmap.end());
  }

  // Sends the PNG image to the layout test framework for comparison.
  void SendPng() {
    // Send image header and |hash_| to the layout test framework.
    std::cout << "Content-Type: image/png\n";
    std::cout << "ActualHash: " << base::MD5DigestToBase16(hash_) << "\n";
    std::cout << "Content-Length: " << png_output_.size() << "\n";

    std::copy(png_output_.begin(),
              png_output_.end(),
              std::ostream_iterator<unsigned char>(std::cout, ""));

    std::cout << "#EOF\n";
    std::cout.flush();
    std::cerr << "#EOF\n";
    std::cerr.flush();
  }

  // Duplicates the tab that was created when the browser opened. This is done
  // so that the observer can listen to the duplicated tab as soon as possible
  // and start listening for messages related to print preview.
  void DuplicateTab() {
    WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(tab);

    print_preview_observer_ = std::make_unique<PrintPreviewObserver>(
        browser(), tab, pdf_file_save_path_);
    chrome::DuplicateTab(browser());

    WebContents* initiator =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(initiator);
    ASSERT_NE(tab, initiator);
  }

  // Resets the test so that another web page can be printed. It also deletes
  // the duplicated tab as it isn't needed anymore.
  void Reset() {
    png_output_.clear();
    ASSERT_EQ(2, browser()->tab_strip_model()->count());
    chrome::CloseTab(browser());
    ASSERT_EQ(1, browser()->tab_strip_model()->count());
  }

  // Creates a temporary directory to store a text file that will be used for
  // stdin to accept input from the layout test framework. A path for the PDF
  // file is also created. The directory and files within it are automatically
  // cleaned up once the test ends.
  void SetupStdinAndSavePath() {
    // Sets the filemode to binary because it will force |std::cout| to send LF
    // rather than CRLF. Sending CRLF will cause an error message for the
    // layout tests.
#if defined(OS_WIN)
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif
    // Sends a message to the layout test framework indicating indicating
    // that the browser test has completed setting itself up. The layout
    // test will then expect the file path for stdin.
    base::FilePath stdin_path;
    std::cout << "#READY\n";
    std::cout.flush();

    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        base::CreateTemporaryFileInDir(tmp_dir_.GetPath(), &stdin_path));

    // Redirects |std::cin| to the file |stdin_path|. |in| is not freed because
    // if it goes out of scope, |std::cin.rdbuf| will be freed, causing an
    // error.
    std::ifstream* in = new std::ifstream(stdin_path.value().c_str());
    ASSERT_TRUE(in->is_open());
    std::cin.rdbuf(in->rdbuf());

    pdf_file_save_path_ =
        tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("dummy.pdf"));

    // Send the file path to the layout test framework so that it can
    // communicate with this browser test.
    std::cout << "StdinPath: " << stdin_path.value() << "\n";
    std::cout << "#EOF\n";
    std::cout.flush();
  }

 private:
  // Generates a png from bitmap data and stores it in |png_output_|.
  void CreatePng(const std::vector<uint8_t>& bitmap_data,
                 int width,
                 int height) {
    base::MD5Sum(static_cast<const void*>(bitmap_data.data()),
                 bitmap_data.size(),
                 &hash_);
    gfx::Rect png_rect(width, height);

    // tEXtchecksum looks funny, but that's what the layout test framework
    // expects.
    std::string comment_title("tEXtchecksum\x00");
    gfx::PNGCodec::Comment hash_comment(comment_title,
                                        base::MD5DigestToBase16(hash_));
    std::vector<gfx::PNGCodec::Comment> comments;
    comments.push_back(hash_comment);
    ASSERT_TRUE(gfx::PNGCodec::Encode(bitmap_data.data(),
                                      gfx::PNGCodec::FORMAT_BGRA,
                                      png_rect.size(),
                                      png_rect.size().width() * kColorChannels,
                                      false,
                                      comments,
                                      &png_output_));
  }

  std::unique_ptr<PrintPreviewObserver> print_preview_observer_;
  base::FilePath pdf_file_save_path_;

  // Vector for storing the PNG to be sent to the layout test framework.
  // TODO(ivandavid): Eventually change this to uint32_t and make everything
  // work with that. It might be a bit tricky to fix everything to work with
  // uint32_t, but not too tricky.
  std::vector<unsigned char> png_output_;

  // Image hash of the bitmap that is turned into a PNG. The hash is put into
  // the PNG as a comment, as it is needed by the layout test framework.
  base::MD5Digest hash_;

  // Temporary directory for storing the pdf and the file for stdin. It is
  // deleted by the layout tests.
  // TODO(ivandavid): Keep it as a ScopedTempDir and change the layout test
  // framework so that it tells the browser test how many test files there are.
  base::ScopedTempDir tmp_dir_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewPdfGeneratedBrowserTest);
};

// This test acts as a driver for the layout test framework.
IN_PROC_BROWSER_TEST_F(PrintPreviewPdfGeneratedBrowserTest,
                       MANUAL_LayoutTestDriver) {
  // What this code is supposed to do:
  // - Setup communication with the layout test framework
  // - Print webpage to a pdf
  // - Convert pdf to a png
  // - Send png to layout test framework, where it doesn an image diff
  //   on the image sent by this test and a reference image.
  //
  // Throughout this code, there will be |std::cout| statements. The layout test
  // framework uses stdout to get data from the browser test and uses stdin
  // to send data to the browser test. Writing "EOF\n" to |std::cout| indicates
  // that whatever block of data that the test was expecting has been completely
  // sent. Sometimes EOF is printed to stderr because the test will expect it
  // from stderr in addition to stdout for certain blocks of data.=
  SetupStdinAndSavePath();

  while (true) {
    std::string input;
    while (input.empty()) {
      std::getline(std::cin, input);
      if (std::cin.eof())
        std::cin.clear();
    }

    // If the layout test framework sends "QUIT" to this test, that means there
    // are no more tests for this instance to run and it should quit.
    if (input == "QUIT")
      break;

    base::FilePath::StringType file_extension = FILE_PATH_LITERAL(".pdf");
    base::FilePath::StringType cmd;
#if defined(OS_POSIX)
    cmd = input;
#elif defined(OS_WIN)
    cmd = base::UTF8ToWide(input);
#endif

    DuplicateTab();
    PrintPreviewSettings settings(
        true,
        "",
        false,
        false,
        DEFAULT_MARGINS,
        cmd.find(file_extension) != base::FilePath::StringType::npos);

    // Splits the command sent by the layout test framework. The first command
    // is always the file path to use for the test. The rest isn't relevant,
    // so it can be ignored. The separator for the commands is an apostrophe.
    std::vector<base::FilePath::StringType> cmd_arguments = base::SplitString(
        cmd, base::FilePath::StringType(1, '\''),
        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    ASSERT_GE(cmd_arguments.size(), 1U);
    base::FilePath::StringType test_name(cmd_arguments[0]);
    NavigateAndPrint(test_name, settings);
    PdfToPng();

    // Message to the layout test framework indicating that it should start
    // waiting for the image data, as there is no more text data to be read.
    // There actually isn't any text data at all, however because the layout
    // test framework requires it, a message has to be sent to stop it from
    // waiting for this message and start waiting for the image data.
    std::cout << "#EOF\n";
    std::cout.flush();

    SendPng();
    Reset();
  }
}

}  // namespace printing
