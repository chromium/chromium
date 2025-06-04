document.addEventListener('DOMContentLoaded', function () {
  const fetchDataButton = document.getElementById('fetchData');
  const sendDataButton = document.getElementById('sendData');
  const uploadTrainingDataButton = document.getElementById('uploadTrainingData'); 
  const trainModelButton = document.getElementById('trainModel');
  const loadModelBERTButton = document.getElementById('loadModelBERT');
  const inferSingleButton = document.getElementById('inferSingleButton');
  const inferBatchButton = document.getElementById('inferBatchButton');

  // Inputs for single inference:
  const questionInput = document.getElementById('questionInput');
  const contextInput = document.getElementById('contextInput');
  
  // Input for batch inference (expects JSON array):
  const batchInput = document.getElementById('batchInput');

  if (fetchDataButton) {
    fetchDataButton.addEventListener('click', () => {
      chrome.readServer.readData((response) => {
        try {
          const parsedData = JSON.parse(response);
          if (parsedData.message) {
            console.log('Data fetched from server:', parsedData.message);
            alert('Data fetched: ' + parsedData.message);
          } else {
            alert('No message found in the response');
          }
        } catch (e) {
          console.error('Failed to parse response:', e);
        }
      });
    });
  } else {
    console.error("Button with ID 'fetchData' not found in DOM.");
  }

  if (sendDataButton) {
    sendDataButton.addEventListener('click', () => {
      const message = "Hello Server!";
      chrome.readServer.sendData(message, (response) => {
        try {
          const parsedResponse = JSON.parse(response);
          if (parsedResponse.response) {
            console.log('Server responded:', parsedResponse.response);
            alert('Server responded: ' + parsedResponse.response);
          } else {
            console.error('Error:', parsedResponse.error);
            alert('Error: ' + parsedResponse.error);
          }
        } catch (e) {
          console.error('Failed to parse server response:', e);
        }
      });
    });
  } else {
    console.error("Button with ID 'sendData' not found in DOM.");
  }

  if (uploadTrainingDataButton) {
    uploadTrainingDataButton.addEventListener('click', () => {
      chrome.readServer.uploadTrainingData((response) => {
        if (response) {
          console.log('Training data upload response:', response);
          alert('Training data uploaded successfully: ' + response);
        } else {
          console.error('Error uploading training data.');
          alert('Error: Failed to upload the training data.');
        }
      });
    });
  } else {
    console.error("Button with ID 'uploadTrainingData' not found in DOM.");
  }

  if (trainModelButton) {
    trainModelButton.addEventListener('click', () => {
      chrome.readServer.trainModel((response) => {
        if (response) {
          console.log('Training response:', response);
          alert('Model training initiated: ' + response);
        } else {
          console.error('Error initiating model training.');
          alert('Error: Failed to start model training.');
        }
      });
    });
  } else {
    console.error("Button with ID 'trainModel' not found in DOM.");
  }

  // New code for loading MobileBERT model.
  if (loadModelBERTButton) {
    loadModelBERTButton.addEventListener('click', () => {
      chrome.readServer.loadModelBERT((response) => {
        try {
          const parsedResponse = JSON.parse(response);
          if (parsedResponse.status) {
            console.log('Model loaded:', parsedResponse.status);
            alert('Model loaded: ' + parsedResponse.status);
          } else {
            console.error('Error:', parsedResponse.error);
            alert('Error: ' + parsedResponse.error);
          }
        } catch (e) {
          console.error('Failed to parse load model response:', e);
          alert('Error: Failed to parse load model response');
        }
      });
    });
  } else {
    console.error("Button with ID 'loadModelBERT' not found in DOM.");
  }

  // New code for single inference.
  if (inferSingleButton && questionInput && contextInput) {
    inferSingleButton.addEventListener('click', () => {
      const question = questionInput.value.trim();
      const context = contextInput.value.trim();
      if (!question || !context) {
        alert('Please provide both a question and a context.');
        return;
      }
      const payload = { question: question, context: context };
      const jsonPayload = JSON.stringify(payload);
      console.log(jsonPayload)
      chrome.readServer.inferSingleBERT(jsonPayload, (response) => {
        try {
          const parsedResponse = JSON.parse(response);
          if (parsedResponse.answer) {
            console.log('Inference result:', parsedResponse.answer);
            alert('Answer: ' + parsedResponse.answer);
          } else {
            console.error('Error:', parsedResponse.error);
            alert('Error: ' + parsedResponse.error);
          }
        } catch (e) {
          console.error('Failed to parse inference response:', e);
          alert('Error: Failed to parse inference response');
        }
      });
    });
  } else {
    console.error("Single inference elements not found in DOM.");
  }

  // New code for batch inference.
  if (inferBatchButton && batchInput) {
    inferBatchButton.addEventListener('click', () => {
      let batchData;
      try {
        batchData = JSON.parse(batchInput.value.trim());
      } catch (e) {
        alert('Invalid JSON for batch input. Please provide a valid JSON array.');
        return;
      }
      const jsonPayload = JSON.stringify(batchData);
      chrome.readServer.inferBatchBERT(jsonPayload, (response) => {
        try {
          const parsedResponse = JSON.parse(response);
          console.log('Batch inference result:', parsedResponse);
          alert('Batch Inference Result: ' + JSON.stringify(parsedResponse));
        } catch (e) {
          console.error('Failed to parse batch inference response:', e);
          alert('Error: Failed to parse batch inference response');
        }
      });
    });
  } else {
    console.error("Batch inference elements not found in DOM.");
  }
});

